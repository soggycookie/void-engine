#pragma once
#include "arena.h"
#include "logger/log.h"
#include "utils.h"
#include <source_location>

namespace VoidEngine
{
class TLFS
{
private:
#ifdef VOID_DEBUG
    struct DebugTag
    {
        DebugTag() : file_(nullptr), line_(0), magic_(DEAD_MAGIC_NUMBER)
        {
        }

        const char *file_;
        int32_t line_;
        int32_t magic_;
    };

    static constexpr uint32_t MAGIC_NUMBER = 0xA7F3C91D;
    static constexpr uint32_t DEAD_MAGIC_NUMBER = 0xDEADDEAD;
#endif // VOID_DEBUG

    struct Header
    {
        uint64_t size_;
#ifdef VOID_DEBUG
        DebugTag debug_tag_;

        Header() : size_(0), debug_tag_()
        {
        }
#else
        Header() : size(0)
        {
        }
#endif // VOID_DEBUG
    };

    struct Block
    {
        Header header_;
        Block *next_free_;
        Block *prev_free_;
    };

    struct Index
    {
        uint16_t fli_;
        uint16_t sli_;

        DEFAULT_COMPARABLE(Index);
    };

private:
#ifdef VOID_DEBUG
    static constexpr uint32_t MIN_BLOCK_SIZE = 64;
#else
    static constexpr uint32_t MIN_BLOCK_SIZE = 32;
#endif // VOID_DEBUG
    static constexpr uint32_t MIN_PAYLOAD_SIZE = MIN_BLOCK_SIZE - sizeof(Header);
    static constexpr uint32_t SMALL_BUCKET_SIZE = 256; // bucket 1
    static constexpr uint32_t SL_BITSHIFT = 5;
    static constexpr uint32_t SL_COUNT = 1U << SL_BITSHIFT;
    // fl_bitmap_ is uint32_t, so FL_COUNT must not exceed 32
    static constexpr uint32_t FL_COUNT = 32;
    static_assert(FL_COUNT <= 32, "fl_bitmap_ is 32 bits wide!");
    static constexpr uint32_t FREELIST_COUNT = FL_COUNT * SL_COUNT;
    // fli = exp - floor_log2(SMALL_BUCKET_SIZE) + 1, so the last fli (FL_COUNT - 1) covers
    // [2^(first_fli + FL_COUNT - 1), 2^(first_fli + FL_COUNT)). Any block (header included) of at
    // most 2^(first_fli + FL_COUNT - 1) bytes has a payload the bins can represent.
    static constexpr uint64_t MAX_BLOCK_SIZE = 1ULL << (floor_log2(SMALL_BUCKET_SIZE) + FL_COUNT - 1);
    static constexpr Index INVALID_INDEX = {FL_COUNT, SL_COUNT};

    void *addr_;
    uint32_t sl_bitmaps_[FL_COUNT];
    Block *freelists_[FREELIST_COUNT];
    uint32_t fl_bitmap_;
    uint32_t alloc_count_;
    uint64_t alloc_size_;
    uint64_t commit_size_;
    uint64_t reserve_size_;
    // true when the last physical block (the one ending at the commit dead-end) is free
    bool tail_free_;
#ifdef VOID_DEBUG
    char name[64];
#endif
public:
    TLFS() : fl_bitmap_(0), alloc_count_(0), alloc_size_(0), freelists_(nullptr), commit_size_(0), reserve_size_(0), tail_free_(false), addr_(nullptr)
    {
    }

    NO_COPY_CTOR(TLFS);
    NO_COPY_ASSIGNMENT(TLFS);

    TLFS(TLFS &&other) noexcept
    {
        std::memcpy(sl_bitmaps_, other.sl_bitmaps_, sizeof(sl_bitmaps_));
        std::memcpy(freelists_, other.freelists_, sizeof(freelists_));

        fl_bitmap_ = other.fl_bitmap_;
        alloc_count_ = other.alloc_count_;
        alloc_size_ = other.alloc_size_;
        commit_size_ = other.commit_size_;
        reserve_size_ = other.reserve_size_;
        tail_free_ = other.tail_free_;
#ifdef VOID_DEBUG
        std::memcpy(name, other.name, 64);
#endif // VOID_DEBUG
        addr_ = other.addr_;

        std::memset(other.sl_bitmaps_, 0, sizeof(sl_bitmaps_));
        std::memset(other.freelists_, 0, sizeof(freelists_));
        other.fl_bitmap_ = 0;
        other.commit_size_ = 0;
        other.reserve_size_ = 0;
        other.tail_free_ = false;
        other.alloc_count_ = 0;
        other.alloc_size_ = 0;
        other.addr_ = nullptr;
    }

    // Vallocator &operator=(Vallocator &&other)
    // {
    //     Destroy();
    //
    //     std::memcpy(sl_bitmaps, other.sl_bitmaps, sizeof(sl_bitmaps));
    //     std::memcpy(freelists, other.freelists, sizeof(freelists));
    //
    //     fl_bitmap = other.fl_bitmap;
    //     alloc_count = other.alloc_count;
    //     total_size = other.total_size;
    //     alloc_size = other.alloc_size;
    //     pool_size = other.pool_size;
    //     pool_head = other.pool_head;
    //
    //     std::memset(other.sl_bitmaps, 0, sizeof(sl_bitmaps));
    //     std::memset(other.freelists, 0, sizeof(freelists));
    //
    //     alloc_count = 0;
    //     total_size = 0;
    //     alloc_size = 0;
    //     pool_head = nullptr;
    //
    //     return *this;
    // }

#ifdef VOID_DEBUG
    void *tlfs_alloc(uint64_t size, uint64_t align, std::source_location loc = std::source_location::current());
    void tlfs_free(void *addr, std::source_location loc = std::source_location::current());
    // void *tlfs_realloc(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align,
    //                    std::source_location loc = std::source_location::current());
    void set_name(const char *name);
    void print();
#else
    void *tlfs_alloc(uint64_t size, uint64_t align);
    // void *tlfs_realloc(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align);
    void tlfs_free(void *addr);
#endif // VOID_DEBUG

    uint32_t get_alloc_count() const
    {
        return alloc_count_;
    }
    uint64_t get_alloc_size() const
    {
        return alloc_size_;
    }
    uint64_t get_min_alloc_size() const
    {
        return MIN_PAYLOAD_SIZE;
    }

    void init(uint64_t res_size, uint64_t com_size);
    void destroy();

private:
    void *tlfs_alloc_internal(uint64_t size, uint64_t align);
    // disabled: see review 2026-11-21 finding 14
    // void *tlfs_realloc_internal(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align);
    void tlfs_free_internal(void *addr);

    uint64_t acquire_memory(uint64_t alloc_size);

    Index round_up_to_bin(uint64_t size);
    Index round_down_to_bin(uint64_t size);
    Index find_next_free_index(Index start_index);

    void *get_payload(Block *block);
    Header *get_block_header(Block *block);
    uint64_t calc_payload_size(uint64_t size);
    uint64_t get_payload_size(Block *block);
    uint64_t get_payload_size(Header *header);

    void insert_free_block(Block *block);
    Block *acquire_block_from_freelist(Index index);
    void remove_free_block(Block *block);

    Block *split(Block *block, uint64_t size);
    bool can_split(Block *block, uint64_t size);
    Block *coalesce(Block *prevBlock, Block *block);

    void mark_used(Block *block);
    void mark_free(Block *block);

    Block *get_block_from_payload(void *addr);

    bool is_used_block(Header *size);
    bool is_prev_physical_free(Block *block);
    bool is_next_physical_free(Block *block);
    bool is_address_valid(void *p);
    bool is_header_deadend(Header *header) const;
};

class Pool
{
private:
    struct Node
    {
        Node *next;
    };

private:
    Arena *page_arena;
    Node *head;
    uint64_t alloc_count;
    uint64_t page_count;
    uint64_t size_class;

public:
    Pool() : page_arena(nullptr), head(nullptr), alloc_count(0), page_count(0), size_class(0)
    {
    }

    void init(Arena *arena, uint64_t size_class);
    void *pool_alloc();
    void pool_free(void *addr);

private:
    void acquire_page();
};

template <typename Key, typename Value>
class FlatHashMap;

class Vallocator
{
private:
    constexpr static uint64_t MIN_SIZE_CLASS = 16;
    constexpr static uint64_t MAX_SIZE_CLASS = 4096;
    constexpr static uint64_t POOL_COUNT = floor_log2(MAX_SIZE_CLASS) - floor_log2(MIN_SIZE_CLASS) + 1;
    TLFS tlfs_;
    Arena page_arena_;
    Pool pools_[POOL_COUNT];
    uint64_t alloc_count_;
#ifdef VOID_DEBUG
    FlatHashMap<uint64_t, std::source_location> *debug_map_;
#endif // VOID_DEBUG

public:
    Vallocator() : tlfs_(), page_arena_(), alloc_count_(0)
#ifdef VOID_DEBUG
        , debug_map_(nullptr)
#endif // VOID_DEBUG
    {
    }

    void init(uint64_t tlfs_reserve_size, uint64_t tlfs_commit_size, uint64_t arena_reserve_size,
              uint64_t arena_commit_size);
#ifdef VOID_DEBUG
    void *valloc(uint64_t size, uint64_t align, std::source_location loc = std::source_location::current());
    void vfree(uint64_t size, void *addr, std::source_location loc = std::source_location::current());
#else
    void *valloc(uint64_t size, uint64_t align);
    void vfree(uint64_t size, void *addr);
#endif // VOID_DEBUG
    void destroy();

    TLFS &get_tlfs()
    {
        return tlfs_;
    }
    Arena &get_arena()
    {
        return page_arena_;
    }
};

template <typename T, typename... Args>
T *construct(TLFS &allocator, Args &&...args)
{
    void *addr = allocator.tlfs_alloc(sizeof(T), alignof(T));
    ENGINE_ASSERT(addr, "Failed to construct object!");
    return new (addr) T(std::forward<Args>(args)...);
}

template <typename T>
void destruct(TLFS &allocator, T *data)
{
    if (!data)
    {
        return;
    }
    data->~T();
    allocator.tlfs_free(data);
}

template <typename T, typename... Args>
T *construct(Vallocator &allocator, Args &&...args)
{
    void *addr = allocator.valloc(sizeof(T), alignof(T));
    ENGINE_ASSERT(addr, "Failed to construct object!");
    return new (addr) T(std::forward<Args>(args)...);
}

template <typename T>
void destruct(Vallocator &allocator, T *data)
{
    if (!data)
    {
        return;
    }
    data->~T();
    allocator.vfree(sizeof(T), data);
}

template <typename T>
T *valloc_array(Vallocator &allocator, uint64_t count, std::source_location loc = std::source_location::current())
{
    void *addr = allocator.valloc(sizeof(T) * count, alignof(T), loc);
    ENGINE_ASSERT(addr, "Failed to allocate array!");
    return SCAST(T *, addr);
}

template <typename T>
void valloc_free_array(Vallocator &allocator, T *addr, uint64_t count, std::source_location loc = std::source_location::current())
{
    allocator.vfree(sizeof(T) * count, addr, loc);
}
} // namespace VoidEngine
