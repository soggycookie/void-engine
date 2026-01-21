#include "sparse_set.h"

namespace ECS
{
    void SparseSet::Init(Allocator* allocator, BlockAllocator* pageAllocator)
    {
        m_allocator = allocator;
        m_pageAllocator = pageAllocator;

        m_dense.Init(allocator, sizeof(uint64_t), 1);
        m_sparse.Init(allocator, sizeof(SparsePage), 0);

        static_cast<uint64_t*>(m_dense.GetFirstElement())[0] = 0;
        m_dense.IncreCount();
    }

    uint32_t SparseSet::GetPageIndex(uint64_t id)
    {
        return id >> SparsePageBit;
    }
    uint32_t SparseSet::GetPageOffset(uint64_t id)
    {
        return id & (SparsePageSize - 1);
    }

    SparsePage* SparseSet::GetSparsePage(uint32_t pageIndex)
    {
        assert(m_sparse.GetCount() < pageIndex + 1 && "Sparse pages were not initialized!");
        SparsePage* array = static_cast<SparsePage*>();

        return CAST_OFFSET_ELEMENT(m_sparse.GetArray(), SparsePage, 
                                   m_sparse.GetAlignedElementSize(), pageIndex );
    }

    SparsePage* SparseSet::CreateSparsePage(uint32_t pageIndex)
    {

    }

    void SparseSet::PushBack(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);
        SparsePage* page = GetSparsePage(id);

        if(!page || !page->denseIndex)
        {
            page = CreateSparsePage(pageIndex);
        }

        
    }

}