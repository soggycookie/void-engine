#include "global_persistant_allocator.h"

namespace VoidEngine
{

    size_t GlobalPersistantAllocator::m_bufferSize = 0;

    void* GlobalPersistantAllocator::Alloc(size_t size, size_t align)
    {
        return m_linearAllocator.Alloc(size, align);
    }
    
    void* GlobalPersistantAllocator::Alloc(size_t size)
    {
        return m_linearAllocator.Alloc(size);
    }
    
    void GlobalPersistantAllocator::Clear()
    {
        m_linearAllocator.Clear();
    }
}