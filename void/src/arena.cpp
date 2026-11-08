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
    res_size = align_up(reserve_size, sys_info.alloc_granularity);
    com_size = align_up(commit_size, sys_info.page_size);

    ENGINE_ASSERT(is_power_of_2(res_size), "Reserve size is not power of 2");
    ENGINE_ASSERT(is_power_of_2(com_size), "Commit size is not power of 2");

    void *data = VirtualMemory::reserve(res_size);
    ENGINE_ASSERT(data, "Fail to reserve memory!");
    void *p = VirtualMemory::commit(data, com_size);
    ENGINE_ASSERT(p, "Failed to commit!");
    addr = data;
}

void *Arena::push(uint64_t size, uint64_t align, uint64_t *out_pos)
{
    ENGINE_ASSERT(res_size > 0, "Reserve size cannot be 0!");
    ENGINE_ASSERT(is_power_of_2(align), "Alignment is not power of 2");
    uint64_t aligned_pos = align_up(pos, align);
    ENGINE_ASSERT(aligned_pos <= res_size && size <= res_size - aligned_pos, "Arena capacity exceeded");
    uint64_t next_pos = aligned_pos + size;

    if (next_pos > com_size)
    {
        uint64_t extra_size = next_pos - com_size;
        uint64_t aligned_extra_size = VirtualMemory::align_to_commit_size(extra_size, nullptr);
        ENGINE_ASSERT(com_size + aligned_extra_size <= res_size, "Allocation size exceed reserve size!");
        uint64_t next_commit_pos = std::max(com_size * 2, aligned_extra_size);
        void *p = VirtualMemory::commit(OFFSET_ADDR(addr, com_size), next_commit_pos - com_size);
        ENGINE_ASSERT(p, "Fail to commit!");
        com_size = next_commit_pos;
    }

    if (out_pos)
    {
        *out_pos = aligned_pos;
    }

    void *result = OFFSET_ADDR(addr, aligned_pos);
    pos = next_pos;

    return result;
}

void *Arena::in_place_grow(void *p, uint64_t old_size, uint64_t new_size, uint64_t align)
{
    ENGINE_ASSERT(p, "Old ptr must not be null!");
    ENGINE_ASSERT(new_size > old_size, "New size must be greater than old size!");
    uint64_t data_end_pos = BYTE_OFFSET(p, addr) + old_size;
    uint64_t extra_alloc = new_size - old_size;
    if (data_end_pos == pos && (res_size - com_size) >= extra_alloc)
    {
        pos += extra_alloc;
        return p;
    }
    else
    {
        return push(new_size, align);
    }
}

void Arena::pop_to(uint64_t pos)
{
    ENGINE_ASSERT(res_size > 0, "Reserve size cannot be 0!");
    ENGINE_ASSERT(com_size > 0, "Commit size cannot be 0!");
    ENGINE_ASSERT(pos <= pos, "Position exceeds arena's current posititon!");
    pos = pos;
}

void Arena::clear()
{
    pos = 0;
}

void Arena::destroy()
{
    if (addr)
    {
        VirtualMemory::release(addr);
    }
}

} // namespace VoidEngine
