#pragma once

#include "../ecs_pch.h"
#include "block_allocator.h"
#include "sparse_set.h"

/*
    Allocator for each requested size
*/

namespace ECS
{

    constexpr uint32_t WorldAllocDefaultDense = 64;

    class WorldAllocator
    {
    public:
        void Init();

        void* Alloc(uint32_t size);
        void* AllocN(uint32_t elementSize, uint32_t capacity, uint32_t& newCapacity);
        void* CallocN(uint32_t elementSize, uint32_t capacity, uint32_t& newCapacity);
        void* Calloc(uint32_t size);
        void Free(uint32_t size, void* addr);

        BlockAllocator* GetOrCreateBalloc(uint32_t size);


    public:
        BlockAllocator m_chunks;
        SparseSet<BlockAllocator> m_sparse;
    };
}
