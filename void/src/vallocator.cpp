#include "vallocator.h"
#include "logger/log.h"
#include "virtual_mem.h"
#include "flat_hash_map.h"

namespace VoidEngine
{

#ifdef VOID_DEBUG
void TLFS::set_name(const char *name)
{
    uint64_t len = std::strlen(name);
    ENGINE_ASSERT(len < 64, "Allocator name must not exceed 63 characters!");
    std::memcpy(this->name, name, len);
    this->name[len] = '\0';
}

void TLFS::print()
{
    // TODO: change this to string pool or sth
    std::stringstream ss;

    ss << "\nAllocator: " << name << "\n";
    Header *first_header = SCAST(Header *, addr_);
    Header *header = first_header;

    while (!is_header_deadend(header))
    {
        if (is_used_block(header))
        {
            if (header->debug_tag_.file_)
            {
                ss << "[Used - " << header->debug_tag_.file_ << ":" << header->debug_tag_.line_ << " - "
                   << get_payload_size(header) << "]";
            }
            else
            {
                ss << "[Used - " << get_payload_size(header) << "]";
            }
        }
        else
        {
            ss << "[Free - " << get_payload_size(header) << "]";
        }
        ss << "\n";
        header = OFFSET_T_ADDR(Header *, header, get_payload_size(header) + sizeof(Header));
    }

    ss << "\n";
    std::cout << ss.str() << std::endl;
}
#endif // VOID_DEBUG

void TLFS::init(uint64_t res_size, uint64_t com_size)
{
    ENGINE_ASSERT(res_size >= com_size && commit_size_ > 0, "Commit size is greater than reserve size!");
    std::memset(sl_bitmaps_, 0, sizeof(sl_bitmaps_));
    std::memset(freelists_, 0, sizeof(freelists_));
    tail_free_ = false;

    commit_size_ = VirtualMemory::align_to_commit_size(com_size, &res_size);
    reserve_size_ = res_size;
    addr_ = VirtualMemory::reserve_commit(reserve_size_, commit_size_);
    ENGINE_ASSERT(addr_, "Fail to reserve and commit memory!");
    Block *init_block = new (addr_) Block();
    init_block->header_.size_ = calc_payload_size(commit_size_);
    init_block->next_free_ = nullptr;
    init_block->prev_free_ = nullptr;
    insert_free_block(init_block);
}

void TLFS::destroy()
{
#ifdef VOID_DEBUG
    if(alloc_count_ != 0)
    {
        print();
    }
#endif // VOID_DEBUG
    ENGINE_ASSERT(alloc_count_ == 0, "Allocator has not freed all resources!");
    if (addr_)
    {
        VirtualMemory::release(addr_);
        std::memset(sl_bitmaps_, 0, sizeof(sl_bitmaps_));
        std::memset(freelists_, 0, sizeof(freelists_));
        fl_bitmap_ = 0;
        commit_size_ = 0;
        reserve_size_ = 0;
        tail_free_ = false;
        alloc_count_ = 0;
        alloc_size_ = 0;
        addr_ = nullptr;
    }
}

uint64_t TLFS::acquire_memory(uint64_t alloc_size)
{
    // alloc_size is the padded payload size; the new block also needs its own header
    ENGINE_ASSERT(alloc_size <= reserve_size_ - commit_size_ - sizeof(Header),
                  "Allocation size exceed reserve size!");
    uint64_t aligned_alloc_size =
        VirtualMemory::align_to_commit_size(alloc_size + sizeof(Header), nullptr);
    ENGINE_ASSERT(aligned_alloc_size <= reserve_size_ - commit_size_,
                  "Allocation size exceed reserve size!");
    // grow by at least the request so the new block can hold it, clamped to the reserve
    ENGINE_ASSERT(aligned_alloc_size <= MAX_BLOCK_SIZE, "Allocation size exceed max bin size!");
    // the new free block must stay representable by the bins (see MAX_BLOCK_SIZE)
    uint64_t next_commit_pos = std::min(
        std::min(std::max(commit_size_ * 2, commit_size_ + aligned_alloc_size), reserve_size_),
        commit_size_ + MAX_BLOCK_SIZE);
    uint64_t new_block_size = next_commit_pos - commit_size_;
    void *new_block_addr = OFFSET_ADDR(addr_, commit_size_);
    void *p = VirtualMemory::commit(new_block_addr, new_block_size);
    ENGINE_ASSERT(p, "Failed to commit!");
    commit_size_ = next_commit_pos;

    Block *new_block = new (new_block_addr) Block();
    new_block->next_free_ = nullptr;
    new_block->prev_free_ = nullptr;
    new_block->header_.size_ = calc_payload_size(new_block_size);

    // The block that ended at the old commit end is free: merge it with the new block so that
    // the invariant "no two adjacent free blocks" holds. The boundary tag is the last 8 bytes
    // before new_block_addr. Skip the merge if the result would not fit the bin range.
    Block *free_block = new_block;
    if (tail_free_)
    {
        void *boundary_tag = OFFSET_T_ADDR(void *, new_block_addr, -sizeof(void *));
        Block *prev = nullptr;
        std::memcpy(&prev, boundary_tag, sizeof(void *));
        ENGINE_ASSERT(prev && is_address_valid(prev), "Invalid tail boundary tag!");
        ENGINE_ASSERT(!is_used_block(get_block_header(prev)), "Tail block must be free!");
        ENGINE_ASSERT(RCAST(char *, prev) + sizeof(Header) + get_payload_size(prev) ==
                          RCAST(char *, new_block_addr),
                      "Tail block is not contiguous with the new block!");

        if (get_payload_size(prev) + new_block_size < MAX_BLOCK_SIZE)
        {
            remove_free_block(prev);
            // prev's own prev-free bit is 0 because its predecessor is used (already coalesced)
            free_block = coalesce(prev, new_block);
            new_block->~Block();
        }
    }

    insert_free_block(free_block);

    return get_payload_size(free_block);
}

void *TLFS::get_payload(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");
    return SCAST(void *, RCAST(char *, block) + sizeof(Header));
}

// minus the header size
uint64_t TLFS::calc_payload_size(uint64_t size)
{
    ENGINE_ASSERT(size >= MIN_BLOCK_SIZE, "Block is not met size requirement!");
    return size - sizeof(Header);
}

uint64_t TLFS::get_payload_size(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");

    return block->header_.size_ & ~3ULL;
}

uint64_t TLFS::get_payload_size(Header *header)
{
    ENGINE_ASSERT(header, "Header cannot be null!");

    return header->size_ & ~3ULL;
}

TLFS::Index TLFS::round_down_to_bin(uint64_t size)
{
    if (size < SMALL_BUCKET_SIZE)
    {
        uint16_t sli = SCAST(uint16_t, size / (SMALL_BUCKET_SIZE / SL_COUNT));

        return {0, sli};
    }

    constexpr uint64_t first_fli = floor_log2(SMALL_BUCKET_SIZE);
    uint64_t exp = floor_log2(size);
    uint64_t fl_size_class = (1ULL << exp);

    uint64_t fli = exp - first_fli + 1;
    if (fli >= FL_COUNT)
    {
        return INVALID_INDEX;
    }

    uint16_t sli = SCAST(uint16_t, (size - fl_size_class) >> (exp - SL_BITSHIFT));

    return {SCAST(uint16_t, fli), sli};
}

TLFS::Index TLFS::round_up_to_bin(uint64_t size)
{
    if (size < SMALL_BUCKET_SIZE)
    {
        constexpr uint64_t step = SMALL_BUCKET_SIZE / SL_COUNT;
        uint64_t sli = (size + step - 1) / step;

        if (sli == SL_COUNT)
        {
            return {1, 0};
        }

        return {0, SCAST(uint16_t, sli)};
    }

    constexpr uint64_t first_fli = floor_log2(SMALL_BUCKET_SIZE);
    uint64_t exp = floor_log2(size);
    uint64_t shift = exp - SL_BITSHIFT;
    uint64_t fl_size_class = 1ULL << exp;

    uint64_t fli = exp - first_fli + 1;
    uint64_t sli = ((size - fl_size_class) + ((1ULL << shift) - 1)) >> shift;

    if (sli == SL_COUNT)
    {
        sli = 0;
        ++fli;
    }

    if (fli >= FL_COUNT)
    {
        return INVALID_INDEX;
    }

    return {SCAST(uint16_t, fli), SCAST(uint16_t, sli)};
}

void TLFS::insert_free_block(Block *block)
{
    ENGINE_ASSERT(block, "Block is null!");
    ENGINE_ASSERT(!is_used_block(get_block_header(block)), "Inserting used block is invalid!");

    Index index = round_down_to_bin(get_payload_size(block));

    Block *current = freelists_[index.fli_ * SL_COUNT + index.sli_];
    block->next_free_ = current;
    block->prev_free_ = nullptr;

#ifdef VOID_DEBUG
    block->header_.debug_tag_.magic_ = DEAD_MAGIC_NUMBER;
#endif // VOID_DEBUG

    if (current)
    {
        current->prev_free_ = block;
    }

    fl_bitmap_ |= (1U << index.fli_);
    sl_bitmaps_[index.fli_] |= (1U << index.sli_);

    freelists_[index.fli_ * SL_COUNT + index.sli_] = block;

    mark_free(block);
}

TLFS::Block *TLFS::acquire_block_from_freelist(Index index)
{
    ENGINE_ASSERT(index != INVALID_INDEX, "Index cannot be invalid!");

    Block *block = freelists_[index.fli_ * SL_COUNT + index.sli_];

#ifdef VOID_DEBUG
    if(block && block->header_.debug_tag_.magic_ != DEAD_MAGIC_NUMBER)
    {
        //should never happen
        print();
        LOG_FATAL("Internal", "Invalid block resides in freelist!");
    }
#endif // VOID_DEBUG

    return block;
}

void TLFS::remove_free_block(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");
    ENGINE_ASSERT(!is_used_block(get_block_header(block)), "Block cannot be used!");

    Index index = round_down_to_bin(get_payload_size(block));

    Block *next = block->next_free_;
    Block *prev = block->prev_free_;

    if (freelists_[index.fli_ * SL_COUNT + index.sli_] == block)
    {
        ENGINE_ASSERT(prev == nullptr, "");
        freelists_[index.fli_ * SL_COUNT + index.sli_] = next;
        if (next == nullptr)
        {
            sl_bitmaps_[index.fli_] &= ~(1U << index.sli_);
            if (sl_bitmaps_[index.fli_] == 0)
            {
                fl_bitmap_ &= ~(1U << index.fli_);
            }
        }
    }

    if (next)
    {
        next->prev_free_ = prev;
    }
    if (prev)
    {
        prev->next_free_ = next;
    }
    block->next_free_ = nullptr;
    block->prev_free_ = nullptr;
}

bool TLFS::can_split(Block *block, uint64_t size)
{
    ENGINE_ASSERT(block, "Block cannot be null!");
    ENGINE_ASSERT(!is_used_block(get_block_header(block)), "Block cannot be used!");

    if (size < MIN_PAYLOAD_SIZE || size >= get_payload_size(block) ||
        (get_payload_size(block) - size) < MIN_BLOCK_SIZE)
    {
        return false;
    }

    return true;
}

TLFS::Block *TLFS::split(Block *block, uint64_t size)
{
    ENGINE_ASSERT(block, "Block cannot be null!");
    ENGINE_ASSERT(!is_used_block(get_block_header(block)), "Block cannot be used!");
    ENGINE_ASSERT(size >= MIN_PAYLOAD_SIZE, "Split size must be greater or equal min payload size");
    ENGINE_ASSERT(get_payload_size(block) >= calc_payload_size(MIN_BLOCK_SIZE * 2),
               "Block size must be greater or equal to min block size");

    uint64_t remaining_size = get_payload_size(block) - size;

    ENGINE_ASSERT(remaining_size >= MIN_BLOCK_SIZE, "Remaining size must be greater or equal min block size");

    void *block_payLoad_addr = get_payload(block);
    void *next_block_addr = OFFSET_ADDR(block_payLoad_addr, size);

    Block *new_block = new (next_block_addr) Block();
    new_block->header_.size_ = calc_payload_size(remaining_size);
#ifdef VOID_DEBUG
    new_block->header_.debug_tag_.magic_ = DEAD_MAGIC_NUMBER;
#endif // VOID_DEBUG
    new_block->next_free_ = nullptr;
    new_block->prev_free_ = nullptr;

    // retain the prev-physical-free flag (bit 1); the used bit is 0 here by assertion.
    // The aligned path in tlfs_alloc_internal frees the leading block right after splitting,
    // so this block may be split a second time and must not lose the flag.
    block->header_.size_ = size | (block->header_.size_ & (1ULL << 1));

    return new_block;
}

TLFS::Block *TLFS::coalesce(Block *prev_block, Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");
    ENGINE_ASSERT(!is_used_block(get_block_header(block)), "Block cannot be used!");
    ENGINE_ASSERT(prev_block, "Prev block cannot be null!");
    ENGINE_ASSERT(!is_used_block(get_block_header(prev_block)), "Prev block cannot be used!");

    void *prev_payload = get_payload(prev_block);
    char *prev_payload_end = OFFSET_T_ADDR(char *, prev_payload, get_payload_size(prev_block));

    ENGINE_ASSERT(RCAST(char *, block) == prev_payload_end,
               "Prev and current block are not continguous!");

    uint64_t new_size = get_payload_size(prev_block) + get_payload_size(block) + sizeof(Header);
    prev_block->header_.size_ = new_size;

    return prev_block;
}

TLFS::Index TLFS::find_next_free_index(Index start_index)
{
    ENGINE_ASSERT(start_index.fli_ < FL_COUNT, "FLI is out of bound!");
    ENGINE_ASSERT(start_index.sli_ < SL_COUNT, "SLI is out of bound!");

    uint32_t sl_map = 0;
    if (start_index.sli_ < SL_COUNT - 1)
    {
        sl_map = sl_bitmaps_[start_index.fli_] & (~0U << (start_index.sli_ + 1));
    }

    if (sl_map > 0)
    {
        uint16_t sli = SCAST(uint16_t, std::countr_zero(sl_map));

        return {start_index.fli_, sli};
    }

    // shifting a 32-bit value by 32 is UB, so the last first-level class has no successor
    uint32_t fl_map = 0;
    if (start_index.fli_ < FL_COUNT - 1)
    {
        fl_map = fl_bitmap_ & (~0U << (start_index.fli_ + 1));
    }

    if (fl_map > 0)
    {
        uint16_t fli = SCAST(uint16_t, std::countr_zero(fl_map));
        uint16_t sli = SCAST(uint16_t, std::countr_zero(sl_bitmaps_[fli]));

        return {fli, sli};
    }

    return INVALID_INDEX;
}

void TLFS::mark_used(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");

    block->header_.size_ |= 1UL;
    void *payload = get_payload(block);
    char *payload_end = OFFSET_T_ADDR(char *, payload, get_payload_size(block));
    Header *next_physical_header = RCAST(Header *, payload_end);
    if (is_header_deadend(next_physical_header))
    {
        tail_free_ = false;
        return;
    }
#ifdef VOID_DEBUG
    ENGINE_ASSERT(next_physical_header->debug_tag_.magic_ == MAGIC_NUMBER ||
                    next_physical_header->debug_tag_.magic_ == DEAD_MAGIC_NUMBER, "");
#endif                                        // VOID_DEBUG
    next_physical_header->size_ &= ~(1ULL << 1); // second LSB
}

void TLFS::mark_free(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");

    block->header_.size_ &= ~1UL;

    // add block address as boundary tag
    void *payload = get_payload(block);
    char *payload_end = OFFSET_T_ADDR(char *, payload, get_payload_size(block));

    char *boundary_tag = payload_end - sizeof(void *);
    // type punning
    std::memcpy(boundary_tag, &block, sizeof(void *));

    // announce this block is free to next physical block
    Header *next_physical_header = RCAST(Header *, payload_end);

    if (is_header_deadend(next_physical_header))
    {
        tail_free_ = true;
        return;
    }

#ifdef VOID_DEBUG
    ENGINE_ASSERT(next_physical_header->debug_tag_.magic_ == MAGIC_NUMBER, "");
#endif                                       // VOID_DEBUG
    next_physical_header->size_ |= (1ULL << 1); // second LSB
}

TLFS::Block *TLFS::get_block_from_payload(void *addr)
{
    return OFFSET_T_ADDR(Block *, addr, -sizeof(Header));
}

bool TLFS::is_used_block(Header *header)
{
    ENGINE_ASSERT(header, "Header cannot be null!");
    return (header->size_ & 1UL) > 0;
}

bool TLFS::is_prev_physical_free(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");
    return (block->header_.size_ & (1UL << 1)) > 0;
}

bool TLFS::is_next_physical_free(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");
    void *payload = get_payload(block);
    char *payload_end = OFFSET_T_ADDR(char *, payload, get_payload_size(block)); // masked overload
    Header *next_header = RCAST(Header *, payload_end);
    if (is_header_deadend(next_header))
    {
        return false;
    }
    return !is_used_block(next_header);
}

bool TLFS::is_address_valid(void *ptr)
{
    return is_address_in_range(ptr, addr_, reserve_size_);
}

bool TLFS::is_header_deadend(Header* header) const
{
    void *deadend = OFFSET_ADDR(addr_, commit_size_);
    return BYTE_OFFSET(deadend, header) == 0;
}

TLFS::Header *TLFS::get_block_header(Block *block)
{
    ENGINE_ASSERT(block, "Block cannot be null!");

    return RCAST(Header *, block);
}

void *TLFS::tlfs_alloc_internal(uint64_t size, uint64_t align)
{
    ENGINE_ASSERT(is_power_of_2(align), "Alignment must be power of 2!");
    if (size == 0)
    {
        return nullptr;
    }

    size = std::max(align_up(size, DEFAULT_ALIGNMENT), SCAST(uint64_t, MIN_PAYLOAD_SIZE));
    ENGINE_ASSERT(size <= reserve_size_, "Size exceeds reserve size!");
    uint64_t padded_size = size;

    Index index = round_down_to_bin(size);
    // size >= 512
    // [256, 512) offset between sl block is 8, so finding next free index is wasteful
    if (align > DEFAULT_ALIGNMENT)
    {
        padded_size = size + align + MIN_BLOCK_SIZE;
        index = round_down_to_bin(padded_size);
    }
    else
    {
        align = DEFAULT_ALIGNMENT;
    }

    if (padded_size >= SMALL_BUCKET_SIZE * 2)
    {
        index = find_next_free_index(index);
    }

    Block *block = nullptr;
    do
    {
        if (index == INVALID_INDEX)
        {
            uint64_t expanded_size = acquire_memory(padded_size);
            index = round_down_to_bin(expanded_size);
        }

        block = acquire_block_from_freelist(index);

        if (block == nullptr)
        {
            index = find_next_free_index(index);
            continue;
        }

        break;

    } while (true);

    ENGINE_ASSERT(block, "Block is null");
    ENGINE_ASSERT(block->header_.debug_tag_.magic_ == DEAD_MAGIC_NUMBER, "Block is corrupted!");

    void *block_payload = get_payload(block);
    void *aligned = align_up(block_payload, align);

    uint64_t byte_offset = BYTE_OFFSET(aligned, block_payload);

    Block *allocated_block = nullptr;

    remove_free_block(block);
    //acquired block is already removed from freelist
    if (byte_offset == 0)
    {
        allocated_block = block;
    }
    else
    {
        // Force to split a leading block
        while (byte_offset < MIN_BLOCK_SIZE)
        {
            aligned = align_up(RCAST(char *, aligned) + 1, align);
            byte_offset = BYTE_OFFSET(aligned, block_payload);
        }

        // Minus next Block::header to get actual size of left block
        if (can_split(block, byte_offset - sizeof(Header)))
        {
            allocated_block = split(block, byte_offset - sizeof(Header));
            insert_free_block(block);
        }
        else
        {
            ENGINE_ASSERT(false, "Failed to split!");
        }
    }

    ENGINE_ASSERT(get_payload_size(allocated_block) >= size,
               "Allocated block does not meet requested size!");

    if (can_split(allocated_block, size))
    {
        Block *new_free_block = split(allocated_block, size);
        insert_free_block(new_free_block);
    }

    ++alloc_count_;
    alloc_size_ += (get_payload_size(allocated_block) + sizeof(Header));
    mark_used(allocated_block);

    // Print();

    return get_payload(allocated_block);
}

// void *TLFS::tlfs_realloc_internal(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align)
// {
//     ENGINE_ASSERT(old_ptr, "Old address must not be null!");
//     ENGINE_ASSERT(is_address_valid(old_ptr), "Address is not belonged to the allocator!");
//     Block *block = get_block_from_payload(old_ptr);
//     uint64_t size = get_payload_size(block);
//     if (size >= new_size)
//     {
//         return old_ptr;
//     }
//     bool is_next_free = is_next_physical_free(block);
//     if (!is_next_free)
//     {
//         return tlfs_alloc_internal(new_size, align);
//     }
//
//     void *payload = get_payload(block);
//     char *payload_end = OFFSET_T_ADDR(char *, payload, size);
//     Block *next = RCAST(Block *, payload_end);
//
//     uint64_t next_size = get_payload_size(next);
//     uint64_t total_size = size + next_size + sizeof(Header);
//     if (total_size < new_size)
//     {
//         return tlfs_alloc_internal(new_size, align);
//     }
//
//     remove_free_block(next);
//     Block *relloc_block = coalesce(block, next);
//     mark_used(relloc_block);
//
//     return get_payload(relloc_block);
// }

void TLFS::tlfs_free_internal(void *addr)
{
    if (!addr)
    {
        return;
    }

    ENGINE_ASSERT(is_address_valid(addr), "Address is not belonged to the allocator!");

    Block *block = get_block_from_payload(addr);
    block->header_.size_ &= ~1UL;
    block->next_free_ = nullptr;
    block->prev_free_ = nullptr;

    bool is_prev_free = is_prev_physical_free(block);
    bool is_next_free = is_next_physical_free(block);

    Block *new_free_block = block;

    --alloc_count_;
    alloc_size_ -= (get_payload_size(block) + sizeof(Header));

    if (is_prev_free)
    {
        void *boundary_tag = OFFSET_T_ADDR(void *, block, -sizeof(void *));
        void *prev_addr = nullptr;
        std::memcpy(&prev_addr, boundary_tag, sizeof(void *));
        Block *prev = RCAST(Block *, prev_addr);

        remove_free_block(prev);
        new_free_block = coalesce(prev, block);

        block->~Block();
    }

    if (is_next_free)
    {
        void *payload = get_payload(block);
        char *payload_end = OFFSET_T_ADDR(char *, payload, get_payload_size(block));
        Block *next = RCAST(Block *, payload_end);

        remove_free_block(next);
        new_free_block = coalesce(new_free_block, next);

        next->~Block();
    }

    insert_free_block(new_free_block);
    // Print();
}

#ifdef VOID_DEBUG

void *TLFS::tlfs_alloc(uint64_t size, uint64_t align, std::source_location loc)
{
    void* payload = tlfs_alloc_internal(size, align);
    if(!payload)
    {
        return nullptr;
    }
    Block* block = get_block_from_payload(payload);
    block->header_.debug_tag_.file_ = loc.file_name();
    block->header_.debug_tag_.line_ = loc.line();
    block->header_.debug_tag_.magic_ = MAGIC_NUMBER;

    return payload;
}

// void *TLFS::tlfs_realloc(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align,
//                            std::source_location loc)
// {
//     void* payload = tlfs_realloc_internal(old_ptr, old_size, new_size, align);
//     if(!payload)
//     {
//         return nullptr;
//     }
//     Block* block = get_block_from_payload(payload);
//     block->header_.debug_tag_.file_ = loc.file_name();
//     block->header_.debug_tag_.line_ = loc.line();
//     block->header_.debug_tag_.magic_ = MAGIC_NUMBER;
//
//     return payload;
// }

void TLFS::tlfs_free(void *addr, std::source_location loc)
{
    if (!addr)
    {
        return;
    }

    ENGINE_ASSERT(is_address_valid(addr), "Address is not belonged to the allocator!");
    Block *block = get_block_from_payload(addr);
    block->header_.size_ &= ~1UL;
    if(block->header_.debug_tag_.magic_ != MAGIC_NUMBER)
    {
        //double free
        print();
        LOG_FATAL_EX("Internal", loc.file_name(), loc.line(), "Double free or corrupted block!");
    }

    tlfs_free_internal(addr);
}
#else
void *TLFS::tlfs_alloc(uint64_t size, uint64_t align)
{
    return tlfs_alloc_internal(size, align);
}

// void *TLFS::tlfs_realloc(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align)
// {
//     return tlfs_realloc_internal(old_ptr, old_size, new_size, align);
// }

void TLFS::tlfs_free(void *addr)
{
    tlfs_free_internal(addr);
}
#endif // VOID_DEBUG

void Pool::init(Arena *arena, uint64_t size_class)
{
    ENGINE_ASSERT(arena, "Arena must not be null!");
    ENGINE_ASSERT(is_power_of_2(size_class), "Size class must be power of 2!");
    this->page_arena = arena;
    this->size_class = size_class;
    acquire_page();
}

void *Pool::pool_alloc()
{
    if (!head)
    {
        acquire_page();
    }
    ENGINE_ASSERT(head, "Failed to allocate more page!");
    Node *node = head;
    head = node->next;
    node->~Node();
    ++alloc_count;
    return node;
}

void Pool::pool_free(void *addr)
{
    if (!addr)
    {
        return;
    }
    ENGINE_ASSERT(
        is_address_in_range(addr, page_arena->get_base_address(), page_arena->get_commit_size()),
        "Address is not belonged to the region!");
    Node *node = new (addr) Node();
    node->next = head;
    head = node;
    --alloc_count;
}

void Pool::acquire_page()
{
    const static uint64_t page_size = VirtualMemory::get_page_size();
    uint64_t num_node = page_size / size_class;

    void *p = page_arena->push(page_size, page_size, nullptr);
    ENGINE_ASSERT(p, "Failed to allocate a page!");
    for (uint32_t idx = 0; idx < num_node; ++idx)
    {
        void *addr = OFFSET_ADDR(p, (idx * size_class));
        Node *node = new (addr) Node();
        node->next = head;
        head = node;
    }
    ++page_count;
}

void Vallocator::init(uint64_t tlfs_res_size, uint64_t tlfs_com_size, uint64_t arena_res_size,
          uint64_t arena_com_size)
{
    tlfs_.init(tlfs_res_size, tlfs_com_size);
    page_arena_.init(arena_res_size, arena_com_size);

    uint64_t size_class = MIN_SIZE_CLASS;
    for (uint32_t idx = 0; idx < POOL_COUNT; ++idx)
    {
        pools_[idx].init(&page_arena_, size_class);
        size_class *= 2;
    }
#ifdef VOID_DEBUG
    debug_map_ = new FlatHashMap<uint64_t, std::source_location>();
    debug_map_->init(tlfs_, 1024);
#endif // VOID_DEBUG
}

#ifdef VOID_DEBUG
void *Vallocator::valloc(uint64_t size, uint64_t align, std::source_location loc)
{
    ENGINE_ASSERT(size >= align && align > 0, "Alignment %llu must be smaller or equal to size %llu", to_ull(align),
                  to_ull(size));
    ++alloc_count_;
    if (size > MAX_SIZE_CLASS)
    {
        void *addr = tlfs_.tlfs_alloc(size, align, loc);
#ifdef VOID_DEBUG
        uint64_t p_int = RCAST(uint64_t, addr);
        debug_map_->insert(tlfs_, p_int, loc);
#endif // VOID_DEBUG

        return addr;
    }
    size = std::max(next_power_of_2(size), MIN_SIZE_CLASS);
    // std::cout << loc.function_name() << ":" << loc.line() << " allocate " << size << std::endl;
    uint32_t idx = floor_log2(size) - floor_log2(MIN_SIZE_CLASS);

    void *addr = pools_[idx].pool_alloc();
#ifdef VOID_DEBUG
        uint64_t p_int = RCAST(uint64_t, addr);
        debug_map_->insert(tlfs_, p_int, loc);
#endif // VOID_DEBUG

        return addr;
}

void Vallocator::vfree(uint64_t size, void *addr,std::source_location loc)
{
    if (!addr)
    {
        return;
    }

#ifdef VOID_DEBUG
    uint64_t p_int = RCAST(uint64_t, addr);
    ENGINE_ASSERT(debug_map_->contain_key(p_int), "Invalid address!");
    debug_map_->remove(p_int);
#endif // VOID_DEBUG;

    --alloc_count_;
    if (size > MAX_SIZE_CLASS)
    {
        tlfs_.tlfs_free(addr, loc);
        return;
    }
    size = std::max(next_power_of_2(size), MIN_SIZE_CLASS);
    // std::cout << loc.function_name() << ":" << loc.line() << " free " << size << std::endl;
    uint32_t idx = floor_log2(size) - floor_log2(MIN_SIZE_CLASS);
    pools_[idx].pool_free(addr);
}
#else
void *Vallocator::valloc(uint64_t size, uint64_t align)
{
    ENGINE_ASSERT(size >= align, "Alignment %llu must be smaller or equal to size %llu", to_ull(align),
                  to_ull(size));
    ++alloc_count;
    if (size > MAX_SIZE_CLASS)
    {
        return tlfs.tlfs_alloc(size, align);
    }
    size = std::max(next_power_of_2(size), MIN_SIZE_CLASS);
    uint32_t idx = floor_log2(size) - floor_log2(MIN_SIZE_CLASS);
    return pools[idx].pool_alloc();
}

void Vallocator::vfree(uint64_t size, void *addr)
{
    if (!addr)
    {
        return;
    }
    --alloc_count;
    if (size > MAX_SIZE_CLASS)
    {
        tlfs.tlfs_free(addr);
        return;
    }
    size = std::max(next_power_of_2(size), MIN_SIZE_CLASS);
    uint32_t idx = floor_log2(size) - floor_log2(MIN_SIZE_CLASS);
    pools[idx].pool_free(addr);
}
#endif // VOID_DEBUG

void Vallocator::destroy()
{
    ENGINE_ASSERT(alloc_count_ == 0, "Allocation count is %llu . Pls free all resources!", alloc_count_);
#ifdef VOID_DEBUG
    if (alloc_count_ != 0 || debug_map_->get_count() != 0)
    {
        for (auto it = debug_map_->begin(); it != debug_map_->end(); ++it) {
            if (it.is_occupied())
            {
                const std::source_location &loc = it.get_val();
                void *addr = RCAST(void *, it.get_key());
                std::cerr << loc.file_name() << std::endl;
                std::cerr << loc.function_name() << "::" << loc.line() << " is not free!" << std::endl;
            }
        }
    }
    debug_map_->destroy(tlfs_);
    delete debug_map_;
#endif // VOID_DEBUG
    tlfs_.destroy();
    page_arena_.destroy();
}
} // namespace VoidEngine
