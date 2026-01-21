#include "ds/block_allocator.h"

namespace ECS
{
    void BlockAllocator::Init(uint32_t size)
    {
        assert(size && "Data Size is 0!");
        dataSize = size;
        chunkSize = Align(size, MinChunkAlign);
        chunkCount = std::max<uint32_t>(PageSize / chunkSize, 1);
        blockSize = chunkCount * chunkSize;
        blockHead = nullptr;
        chunkHead = nullptr;
    }

    void* BlockAllocator::Alloc()
    {
        if(chunkCount <= MinChunkCount)
        {
            return std::malloc(dataSize);
        }

        if(!chunkHead)
        {
            chunkHead = CreateBlock();
            assert(chunkHead && "Chunk Head is null!");
        }

        chunkHead = chunkHead->next;

        return chunkHead;
    }

    void BlockAllocator::Free(void* addr)
    {
        if(chunkCount <= MinChunkCount)
        {
            std::free(addr);
            return;
        }

        BlockAllocatorChunk* freeChunk = static_cast<BlockAllocatorChunk*>(addr);

        freeChunk->next = chunkHead;
        chunkHead = freeChunk;
    }

    BlockAllocatorChunk* BlockAllocator::CreateBlock()
    {
        BlockAllocatorBlock* block = 
            static_cast<BlockAllocatorBlock*>(
                std::malloc(sizeof(BlockAllocatorBlock) + chunkCount * chunkSize)
            );

        assert(block && "Malloc block is null!");

        BlockAllocatorChunk* firstChunk = reinterpret_cast<BlockAllocatorChunk*>(
                reinterpret_cast<uintptr_t>(block) + sizeof(BlockAllocatorBlock)
            );

        block->firstChunk = firstChunk;
        block->next = blockHead;
        blockHead = block;

        chunkHead = firstChunk;

        BlockAllocatorChunk* chunk = firstChunk;
        for(uint32_t i = 1; i < chunkCount; ++i)
        {
            chunk->next = reinterpret_cast<BlockAllocatorChunk*>(
                    reinterpret_cast<uintptr_t>(block->firstChunk) + chunkSize * i
                );
            chunk = chunk->next;
        }

        chunk->next = nullptr;

        return firstChunk;
    }

    void BlockAllocator::Clear()
    {
        if(chunkCount <= MinChunkCount)
        {
            return;
        }

        BlockAllocatorBlock* block = blockHead;
        while(block)
        {
            BlockAllocatorBlock* nextBlock = block->next;
            
            std::free(block);
            
            block = nextBlock;
        }
    }



}