#include "ds/block_allocator.h"

namespace ECS
{
    void BlockAllocator::Init(uint32_t size)
    {
        assert(size && "Data Size is 0!");
        m_chunkSize = Align(size, MinChunkAlign);
        m_chunkCount = std::max<uint32_t>(PageSize / m_chunkSize, 1);
        m_blockSize = m_chunkCount * m_chunkSize;
        m_blockHead = nullptr;
        m_chunkHead = nullptr;
    }

    void* BlockAllocator::Alloc()
    {
        if(m_chunkCount <= MinChunkCount)
        {
            return std::malloc(m_chunkSize);
        }

        if(!m_chunkHead)
        {
            m_chunkHead = CreateBlock();
            assert(m_chunkHead && "Chunk Head is null!");
        }

        auto chunk = m_chunkHead;
        m_chunkHead = m_chunkHead->next;
        ++m_allocCount;
        
        return chunk;
    }

    void* BlockAllocator::Calloc()
    {
        if(m_chunkCount <= MinChunkCount)
        {
            return std::calloc(1, m_chunkSize);
        }

        if(!m_chunkHead)
        {
            m_chunkHead = CreateBlock();
            assert(m_chunkHead && "Chunk Head is null!");
        }

        auto chunk = m_chunkHead;
        m_chunkHead = m_chunkHead->next;

        std::memset(chunk, 0, m_chunkSize);
        ++m_allocCount;

        return chunk;        
    }


    void BlockAllocator::Free(void* addr)
    {
        if(m_chunkCount <= MinChunkCount)
        {
            std::free(addr);
            return;
        }
        
        BlockAllocatorBlock* block = m_blockHead;
        
        while(true)
        {
            assert(block && "Free memory is not belonged to this pool!");

            uintptr_t bStart = RCAST(block, uintptr_t);
            uintptr_t bEnd = bStart + sizeof(BlockAllocatorBlock) + m_chunkCount * m_chunkSize;
            uintptr_t a = RCAST(addr, uintptr_t);
            
            if(a >= bStart && a < bEnd)
            {
                break;
            }

            block = block->next;
        }

        BlockAllocatorChunk* freeChunk = static_cast<BlockAllocatorChunk*>(addr);

        freeChunk->next = m_chunkHead;
        m_chunkHead = freeChunk;

        --m_allocCount;
    }

    BlockAllocatorChunk* BlockAllocator::CreateBlock()
    {
        BlockAllocatorBlock* block = 
            static_cast<BlockAllocatorBlock*>(
                std::malloc(sizeof(BlockAllocatorBlock) + m_chunkCount * m_chunkSize)
            );

        assert(block && "Malloc block is null!");

        BlockAllocatorChunk* firstChunk = reinterpret_cast<BlockAllocatorChunk*>(
                reinterpret_cast<uintptr_t>(block) + sizeof(BlockAllocatorBlock)
            );

        block->firstChunk = firstChunk;
        block->next = m_blockHead;
        m_blockHead = block;

        m_chunkHead = firstChunk;

        BlockAllocatorChunk* chunk = firstChunk;
        for(uint32_t i = 1; i < m_chunkCount; ++i)
        {
            chunk->next = reinterpret_cast<BlockAllocatorChunk*>(
                    reinterpret_cast<uintptr_t>(block->firstChunk) + m_chunkSize * i
                );
            chunk = chunk->next;
        }

        chunk->next = nullptr;

        return firstChunk;
    }

    void BlockAllocator::Destroy()
    {
        BlockAllocatorBlock* block = m_blockHead;
        while(block)
        {
            auto freeBlock = block;
            block = block->next;

            std::free(freeBlock);
        }
    }

}