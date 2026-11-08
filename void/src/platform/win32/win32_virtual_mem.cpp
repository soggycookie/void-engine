#include "virtual_mem.h"
#include "logger/log.h"

#ifdef VOID_WIN32
#include <windows.h>

namespace VoidEngine::VirtualMemory
{

void *reserve(uint64_t size)
{
    ENGINE_ASSERT(size > 0, "Cannot reserve 0 byte!");
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

void *commit(void *addr, uint64_t size)
{
    ENGINE_ASSERT(addr, "Cannot commit null address!");
    ENGINE_ASSERT(size > 0, "Cannot commit 0 byte!");
    return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
}

void *reserve_commit(uint64_t res_size, uint64_t com_size)
{
    ENGINE_ASSERT(res_size > 0, "Cannot reserve 0 byte!");
    LPVOID addr = NULL;
    if (res_size == com_size)
    {
        addr = VirtualAlloc(addr, res_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (addr == NULL)
        {
        }
    }
    else
    {
        addr = reserve(res_size);
        if (addr == NULL)
        {
        }
        else
        {
            if (com_size > 0)
            {
                commit(addr, com_size);
            }
        }
    }
    return addr;
}

void decommit(void *addr, uint64_t size)
{
    ENGINE_ASSERT(size > 0, "Cannot decommit 0 byte!");
    BOOL result = VirtualFree(addr, size, MEM_DECOMMIT);
    if (!result)
    {
    }
}

void release(void *addr)
{
    ENGINE_ASSERT(addr, "Cannot release null address!");
    BOOL result = VirtualFree(addr, 0, MEM_RELEASE);
    if (!result)
    {
    }
}

uint64_t get_page_size()
{
    static uint64_t s_page_size = 0;
    if (s_page_size != 0)
    {
        return s_page_size;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    s_page_size = sysInfo.dwPageSize;

    return s_page_size;
}

SysInfo get_system_info()
{
    static SysInfo info = {0, 0};
    if (info.alloc_granularity != 0 && info.page_size != 0)
    {
        return info;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.page_size = sysInfo.dwPageSize;
    info.alloc_granularity = sysInfo.dwAllocationGranularity;

    return info;
}
} // namespace VoidEngine::VirtualMemory
#endif // VOID_WIN32
