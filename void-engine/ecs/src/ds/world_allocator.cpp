#include "ds/world_allocator.h"

namespace ECS
{
    void WorldAllocator::Init()
    {
        m_chunks.Init(SparsePageSize * sizeof(BlockAllocator));   
        m_sparse.Init(nullptr, &m_chunks, sizeof(BlockAllocator), WorldAllocDefaultDense);
    }

    void* WorldAllocator::Alloc(uint32_t size)
    {
        uint32_t alignedSize = Align(size, 16);

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);

        return block->Alloc();
    }

    void* WorldAllocator::Calloc(uint32_t size)
    {
        uint32_t alignedSize = Align(size, 16);

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);    

        return block->Calloc();
    }
    
    void WorldAllocator::Free(uint32_t size, void* addr)
    {
        uint32_t alignedSize = Align(size, 16);

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);    

        block->Free(addr);
    }
    
    BlockAllocator* WorldAllocator::GetOrCreateBalloc(uint32_t size)
    {
        uint64_t id = size / 16;
        
        uint32_t pageIndex = m_sparse.GetPageIndex(id);
        uint32_t pageOffset = m_sparse.GetPageOffset(id);

        BlockAllocator* block = nullptr;

        if(!m_sparse.isValidDense(id))
        {
            m_sparse.PushBack(id);
            block = new (m_sparse.GetSparsePageData(id)) BlockAllocator();
            block->Init(size);
        }
        else
        {
            block = PTR_CAST(m_sparse.GetSparsePageData(id), BlockAllocator);
        }

        assert(block && "Block allocator is null!");

        return block;
    }

    void* WorldAllocator::AllocN(uint32_t elementSize, uint32_t capacity, uint32_t& newCapacity)
    {
        assert(elementSize || capacity && "Alloc 0 byte!");

        uint32_t alignedSize = Align(elementSize * capacity, 16);

        newCapacity = alignedSize / elementSize;

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);

        return block->Alloc();
    }

    void* WorldAllocator::CallocN(uint32_t elementSize, uint32_t capacity, uint32_t& newCapacity)
    {
        assert(elementSize || capacity && "Alloc 0 byte!");

        uint32_t alignedSize = Align(elementSize * capacity, 16);

        newCapacity = alignedSize / elementSize;

        BlockAllocator* block = GetOrCreateBalloc(alignedSize);

        return block->Calloc();
    }

}