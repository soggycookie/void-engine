#pragma once
#include "utils.h"

namespace VoidEngine::VirtualMemory
{

struct SysInfo
{
    uint64_t page_size;
    uint64_t alloc_granularity;
};

void *reserve(uint64_t size);
void *commit(void *addr, uint64_t size);
void *reserve_commit(uint64_t res_size, uint64_t com_size);
void decommit(void *addr, uint64_t size);
void release(void *addr);

uint64_t get_page_size();
SysInfo get_system_info();

// Returns page-aligned commit size.
// If res_size is non-null, aligns its input value to allocation granularity.
inline uint64_t align_to_commit_size(uint64_t com_size, uint64_t *res_size)
{
    VirtualMemory::SysInfo sys_info = VirtualMemory::get_system_info();
    if (res_size)
    {
        *res_size = align_up(*res_size, sys_info.alloc_granularity);
    }
    return align_up(com_size, sys_info.page_size);
}

} //VirtualMemory
