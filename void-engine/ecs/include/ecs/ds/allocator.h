#pragma once

#include "../ecs_pch.h"
#include "block_allocator.h"
#include "sparse_set.h"

/*
    Allocator for each requested size
*/

namespace ECS
{
    class Allocator
    {
    public:
        void* Alloc(size_t size);
        void Free(size_t size, void* addr);

    public:
        BlockAllocator chunks;
        SparseSet sparse;
    };
}
