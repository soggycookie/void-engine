#include "memory_system.h"

namespace VoidEngine
{
    FreeListAllocator MemorySystem::s_generalAllocator;
    LinearAllocator MemorySystem::s_perFrameAllocator;
    FreeListAllocator MemorySystem::s_resourceLookUpAllocator;
    PoolAllocator MemorySystem::s_resourceAllocator;

    void MemorySystem::StartUp(const EngineConfig& config)
    {
        s_generalAllocator = FreeListAllocator(config.persistantAllocatorSize);
        s_perFrameAllocator         = LinearAllocator(config.perFrameAllocatorSize);
        s_resourceLookUpAllocator   = FreeListAllocator(config.resourceLookUpAllocatorSize);
        s_resourceAllocator         = PoolAllocator(config.resourceAllocatorSize, config.resourceChunkSize, config.resourceAlignment);
    }

    void MemorySystem::ShutDown()
    {
    }
}