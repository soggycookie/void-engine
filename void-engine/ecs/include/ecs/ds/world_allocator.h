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

        void* Init(uint32_t totalSize);
        void* AllocN(uint32_t elementSize, uint32_t capacity, uint32_t& expandedCapacity);
        void* CallocN(uint32_t elementSize, uint32_t capacity, uint32_t& expandedCapacity);
        void* Calloc(uint32_t totalSize);
        void* Alloc(uint32_t totalSize);

        template<typename T>
        T* Alloc(uint32_t count)
        {
            return PTR_CAST(Alloc(sizeof(T) * count), T);
        }
        
        template<typename T>
        T* AllocN(uint32_t count, uint32_t& expandedCapacity)
        {
            return PTR_CAST(AllocN(sizeof(T), count, expandedCapacity), T);
        }

        template<typename T>
        T* Calloc(uint32_t count)
        {
            return PTR_CAST(Calloc(sizeof(T) * count), T);
        }
        
        template<typename T>
        T* CallocN(uint32_t count, uint32_t& expandedCapacity)
        {
            return PTR_CAST(CallocN(sizeof(T), count, expandedCapacity), T);
        }

        void Free(uint32_t size, void* addr);

        BlockAllocator* GetOrCreateBalloc(uint32_t size);


    public:
        BlockAllocator m_chunks;
        SparseSet<BlockAllocator> m_sparse;
    };
}

/*
    Template function definition
*/
#include "sparse_set.inl"
