#pragma once
#include "../ecs_pch.h"

#pragma once
#include "ecs_pch.h"

namespace ECS
{

constexpr uint32_t MinChunkCount = 1;
constexpr uint32_t MinChunkAlign = 16;
constexpr uint32_t PageSize = KB(4);

    struct BlockAllocatorChunk
    {
        BlockAllocatorChunk* next;
    };

    struct BlockAllocatorBlock
    {
        void* firstChunk;
        BlockAllocatorBlock* next;
    };

    class BlockAllocator
    {
    public:
        BlockAllocator()
            : dataSize(0), chunkCount(0), chunkSize(0), blockSize(0),
            chunkHead(nullptr), blockHead(nullptr)
        {
        }

        void Init(uint32_t dataSize);
        void* Alloc();
        void Free(void* addr);

    private:
        BlockAllocatorChunk* CreateBlock();
        void Clear();

    public:
        uint32_t dataSize;
        uint32_t chunkCount;
        uint32_t chunkSize;
        uint32_t blockSize;
        BlockAllocatorChunk* chunkHead;
        BlockAllocatorBlock* blockHead;
    };
}