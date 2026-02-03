#pragma once
#include "../ecs_pch.h"

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
        friend class WorldAllocator;

        BlockAllocator()
            : m_allocCount(0), m_chunkCount(0), m_chunkSize(0), m_blockSize(0),
            m_chunkHead(nullptr), m_blockHead(nullptr)
        {
        }

        void Init(uint32_t dataSize);
        void* Alloc();
        void* Calloc();
        void Free(void* addr);
        void Destroy();

    private:
        BlockAllocatorChunk* CreateBlock();

    private:
        uint32_t m_allocCount;
        uint32_t m_chunkCount;
        uint32_t m_chunkSize;
        uint32_t m_blockSize;
        BlockAllocatorChunk* m_chunkHead;
        BlockAllocatorBlock* m_blockHead;
    };
}