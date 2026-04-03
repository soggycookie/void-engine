#include "ds/world_allocator.h"

namespace ECS
{
void WorldAllocator::Init()
{
    m_chunks.Init(SparsePageCount * sizeof(BlockAllocator));
    m_sparse.Init(nullptr, &m_chunks, WorldAllocDefaultDense, false);
}


void *WorldAllocator::Calloc(uint32_t size)
{
    assert(size != 0);
    uint32_t alignedSize = RoundMinPowerOf2(size, 16);

    BlockAllocator *block = GetOrCreateBalloc(alignedSize);

    return block->Calloc();
}

void *WorldAllocator::Alloc(uint32_t size)
{
    assert(size != 0);
    uint32_t alignedSize = RoundMinPowerOf2(size, 16);

    BlockAllocator *block = GetOrCreateBalloc(alignedSize);

    return block->Alloc();
}

void WorldAllocator::Free(uint32_t size, void *addr)
{
    assert(size != 0);
    uint32_t alignedSize = RoundMinPowerOf2(size, 16);

    BlockAllocator *block = GetOrCreateBalloc(alignedSize);

    block->Free(addr);
}

BlockAllocator *WorldAllocator::GetOrCreateBalloc(uint32_t size)
{
    assert(size != 0);
    uint64_t id = size / 16;
    id = id / 2 + 1;

    uint32_t pageIndex = m_sparse.GetPageIndex(id);
    uint32_t pageOffset = m_sparse.GetPageOffset(id);

    BlockAllocator *block = nullptr;

    if (!m_sparse.IsExisting(id))
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

void *WorldAllocator::AllocN(uint32_t elementSize, uint32_t capacity,
                             uint32_t &expandedCapacity)
{
    assert((elementSize || capacity) && "Alloc 0 byte!");

    uint32_t alignedSize = RoundMinPowerOf2(elementSize * capacity, 16);

    expandedCapacity = alignedSize / elementSize;

    BlockAllocator *block = GetOrCreateBalloc(alignedSize);

    // std::cout << "Alloc " << elementSize * capacity << " using block
    // allocator with chunk size: "
    //     << block->m_chunkSize << ", chunk count: " << block->m_chunkCount <<
    //     std::endl;

    return block->Alloc();
}

void *WorldAllocator::CallocN(uint32_t elementSize, uint32_t capacity,
                              uint32_t &expandedCapacity)
{
    assert((elementSize || capacity) && "Alloc 0 byte!");

    uint32_t alignedSize = RoundMinPowerOf2(elementSize * capacity, 16);

    expandedCapacity = alignedSize / elementSize;

    BlockAllocator *block = GetOrCreateBalloc(alignedSize);

    // std::cout << "Calloc " << elementSize * capacity << " using block
    // allocator with chunk size: "
    //     << block->m_chunkSize << ", chunk count: " << block->m_chunkCount <<
    //     std::endl;

    return block->Calloc();
}

void WorldAllocator::Destroy()
{
    for (uint32_t bIdx = 0; bIdx <= m_sparse.GetCount(); bIdx++)
    {
        BlockAllocator *ba = m_sparse.GetPageData(
            m_sparse.GetId(bIdx));
        
        if(ba)
        {
            ba->Destroy();
        }
    }

    m_sparse.Destroy();
    m_chunks.Destroy();
}

} // namespace ECS
