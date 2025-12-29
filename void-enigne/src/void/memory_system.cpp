#include "memory_system.h"

namespace VoidEngine
{
    FreeListAllocator MemorySystem::s_enginePersistantAllocator;
    LinearAllocator MemorySystem::s_perFrameAllocator;
    FreeListAllocator MemorySystem::s_resourceLookUpAllocator;
    FreeListAllocator MemorySystem::s_resourceStreamAllocator;
    PoolAllocator MemorySystem::s_resourceAllocator;

    void MemorySystem::StartUp(const EngineConfig& config)
    {
        s_enginePersistantAllocator = FreeListAllocator(config.persistantAllocatorSize);
        s_perFrameAllocator         = LinearAllocator(config.perFrameAllocatorSize);
        s_resourceLookUpAllocator   = FreeListAllocator(config.resourceLookUpAllocatorSize);
        s_resourceAllocator         = PoolAllocator(config.resourceAllocatorSize, config.resourceChunkSize, config.resourceAlignment);
        s_resourceStreamAllocator   = FreeListAllocator(config.resourceStreamAllocatorSize);
    }
}