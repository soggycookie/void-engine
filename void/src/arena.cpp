#include "arena.h"
#include "virtual_mem.h"

namespace VoidEngine
{

thread_local ThreadScratch t_scratches;

void Arena::init(uint64_t reserve_size, uint64_t commit_size)
{
    ENGINE_ASSERT(reserve_size > 0, "Reserve size cannot be 0!");
    ENGINE_ASSERT(commit_size > 0, "Commit size cannot be 0!");

    VirtualMemory::SysInfo sys_info = VirtualMemory::get_system_info();
    reserve_size_ = align_up(reserve_size, sys_info.alloc_granularity);
    commit_size_ = align_up(commit_size, sys_info.page_size);

    ENGINE_ASSERT(is_power_of_2(reserve_size_), "Reserve size is not power of 2");
    ENGINE_ASSERT(is_power_of_2(commit_size_), "Commit size is not power of 2");

    void *data = VirtualMemory::reserve(reserve_size_);
    ENGINE_ASSERT(data, "Fail to reserve memory!");
    void *p = VirtualMemory::commit(data, commit_size_);
    ENGINE_ASSERT(p, "Failed to commit!");
    addr_ = data;
}

void Arena::ensure_committed(uint64_t next_pos)
{
    ENGINE_ASSERT(next_pos <= reserve_size_, "Arena capacity exceeded");
    if (next_pos > commit_size_)
    {
        uint64_t required_commit_pos = VirtualMemory::align_to_commit_size(next_pos, nullptr);
        uint64_t next_commit_pos = std::max(commit_size_ * 2, required_commit_pos);
        if (next_commit_pos > reserve_size_)
        {
            LOG_WARN("ARENA", "Commit position %llu exceeds reserve size %llu, clamping",
                     SCAST(unsigned long long, next_commit_pos), to_ull(reserve_size_));
            next_commit_pos = reserve_size_;
            ENGINE_ASSERT(next_pos <= next_commit_pos,
                          "Arena is full. Remaining %llu, but required end position %llu",
                          to_ull(reserve_size_ - commit_size_), to_ull(next_pos));
        }
        void *p =
            VirtualMemory::commit(OFFSET_ADDR(addr_, commit_size_), next_commit_pos - commit_size_);
        ENGINE_ASSERT(p, "Fail to commit!");
        commit_size_ = next_commit_pos;
    }
}

void *Arena::push(uint64_t size, uint64_t align, uint64_t *out_pos)
{
    ENGINE_ASSERT(reserve_size_ > 0, "Reserve size cannot be 0!");
    ENGINE_ASSERT(is_power_of_2(align), "Alignment is not power of 2");
    uint64_t aligned_pos = align_up(pos_, align);
    ENGINE_ASSERT(aligned_pos <= reserve_size_ && size <= reserve_size_ - aligned_pos,
                  "Arena capacity exceeded");
    uint64_t next_pos = aligned_pos + size;

    ensure_committed(next_pos);

    if (out_pos)
    {
        *out_pos = aligned_pos;
    }

    void *result = OFFSET_ADDR(addr_, aligned_pos);
    pos_ = next_pos;

    return result;
}

void *Arena::in_place_grow(void *p, uint64_t old_size, uint64_t new_size, uint64_t align)
{
    ENGINE_ASSERT(p, "Old ptr must not be null!");
    ENGINE_ASSERT(new_size > old_size, "New size must be greater than old size!");
    ENGINE_ASSERT(RCAST(uintptr_t, p) >= RCAST(uintptr_t, addr_),
                  "Pointer does not belong to this arena!");
    uint64_t data_end_pos = BYTE_OFFSET(p, addr_) + old_size;
    uint64_t extra_alloc = new_size - old_size;
    // Tail allocation whose growth still fits in the reserve: extend in place (same start address,
    // so its alignment is unchanged), committing more pages if needed. Otherwise relocate.
    if (data_end_pos == pos_ && extra_alloc <= reserve_size_ - pos_)
    {
        ensure_committed(pos_ + extra_alloc);
        pos_ += extra_alloc;
        return p;
    }
    else
    {
        return push(new_size, align);
    }
}

void Arena::pop_to(uint64_t pos)
{
    ENGINE_ASSERT(reserve_size_ > 0, "Reserve size cannot be 0!");
    ENGINE_ASSERT(commit_size_ > 0, "Commit size cannot be 0!");
    ENGINE_ASSERT(pos_ <= pos, "Position exceeds arena's current posititon!");
    pos_ = pos;
}

void Arena::clear()
{
    pos_ = 0;
}

void Arena::destroy()
{
    if (addr_)
    {
        VirtualMemory::release(addr_);
        addr_ = nullptr;
    }
    commit_size_ = 0;
    reserve_size_ = 0;
    pos_ = 0;
}

} // namespace VoidEngine
