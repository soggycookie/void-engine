#pragma once
#include "pch.h"
#include "engine_config.h"
#include "allocator/linear_allocator.h"
#include "allocator/stack_allocator.h"
#include "allocator/free_list_allocator.h"
#include "allocator/pool_allocator.h"

namespace VoidEngine
{
    class MemorySystem
    {
    public:

        static FreeListAllocator* GeneralAllocator()
        {
            return &s_generalAllocator;
        }

    private:
        friend class Application;

        static void StartUp(const EngineConfig& config);
        static void ShutDown();

    private:
        static FreeListAllocator s_generalAllocator;
        static LinearAllocator s_perFrameAllocator;
        static FreeListAllocator s_resourceLookUpAllocator;
        static PoolAllocator s_resourceAllocator;

    };
}
