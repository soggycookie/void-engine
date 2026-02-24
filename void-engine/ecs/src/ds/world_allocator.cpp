#include "ds/world_allocator.h"

namespace ECS
{
    void WorldAllocator::Init()
    {
        m_chunks.Init(SparsePageCount * sizeof(BlockAllocator));   
        m_sparse.Init(nullptr, &m_chunks, WorldAllocDefaultDense, false);
    }

    void* WorldAllocator::Init(uint32_t size)
    {
        uint32_t alignedSize = RoundMinPowerOf2(size, 16);

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);

        return block->Init();
    }

    void* WorldAllocator::Calloc(uint32_t size)
    {
        uint32_t alignedSize = RoundMinPowerOf2(size, 16);

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);    

        return block->Calloc();
    }
    
    void WorldAllocator::Free(uint32_t size, void* addr)
    {
        uint32_t alignedSize = RoundMinPowerOf2(size, 16);

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);    

        block->Free(addr);
    }
    
    BlockAllocator* WorldAllocator::GetOrCreateBalloc(uint32_t size)
    {
        //pack the size into closer
        // 1 2 3 5
        uint64_t id = size / 16;
        id = id / 2 + 1;

        uint32_t pageIndex = m_sparse.GetPageIndex(id);
        uint32_t pageOffset = m_sparse.GetPageOffset(id);

        BlockAllocator* block = nullptr;

        if(!m_sparse.isValidDense(id))
        {
            m_sparse.PushBack(id, BlockAllocator());
            block = m_sparse.GetPageData(id);
            block->Init(size);
        }
        else
        {
            block = m_sparse.GetPageData(id);
        }

        assert(block && "Block allocator is null!");

        return block;
    }

    void* WorldAllocator::AllocN(uint32_t elementSize, uint32_t capacity, uint32_t& expandedCapacity)
    {
        assert((elementSize || capacity) && "Alloc 0 byte!");

        uint32_t alignedSize = RoundMinPowerOf2(elementSize * capacity, 16);

        expandedCapacity = alignedSize / elementSize;

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);

        //std::cout << "Alloc " << elementSize * capacity << " using block allocator with chunk size: "
        //    << block->m_chunkSize << ", chunk count: " << block->m_chunkCount << std::endl;

        return block->Init();
    }

    void* WorldAllocator::CallocN(uint32_t elementSize, uint32_t capacity, uint32_t& expandedCapacity)
    {
        assert((elementSize || capacity) && "Alloc 0 byte!");

        uint32_t alignedSize = RoundMinPowerOf2(elementSize * capacity, 16);

        expandedCapacity = alignedSize / elementSize;

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);

        //std::cout << "Calloc " << elementSize * capacity << " using block allocator with chunk size: "
        //    << block->m_chunkSize << ", chunk count: " << block->m_chunkCount << std::endl;

        return block->Calloc();
    }

}