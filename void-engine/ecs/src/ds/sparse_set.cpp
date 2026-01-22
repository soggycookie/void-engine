#include "ds/sparse_set.h"
#include "ds/world_allocator.h"

namespace ECS
{
    void SparseSet::Init(WorldAllocator* allocator, BlockAllocator* pageAllocator, 
                         uint32_t elementSize, uint32_t elementAlignment)
    {
        m_allocator = allocator;
        m_pageAllocator = pageAllocator;
        m_alignedElementSize = Align(elementSize, elementAlignment);

        m_dense.Init(allocator, sizeof(uint64_t), alignof(uint64_t), 1);
        m_sparse.Init(allocator, sizeof(SparsePage), alignof(SparsePage), 0);

        CAST(m_dense.GetFirstElement(), uint64_t)[0] = 0;
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
        if(m_sparse.GetCount() < pageIndex + 1)
        {
            return nullptr;
        }

        return CAST_OFFSET_MEM_ARR_ELEMENT(m_sparse, pageIndex, SparsePage);
    }

    SparsePage* SparseSet::CreateSparsePage(uint32_t pageIndex)
    {
        if(m_sparse.GetCount() >= pageIndex + 1)
        {
            return nullptr;
        }

        void* oldArray = m_sparse.GetArray();
        size_t oldSize = m_sparse.GetAlignedElementSize() * m_sparse.GetCapacity();

        m_sparse.Grow(m_allocator, pageIndex + 1);

        SparsePage* newPage = CAST(m_sparse.GetElement(pageIndex), SparsePage);
        
        assert(newPage && "New page failed to create!");

        CallocPageDenseIndex(newPage);
        
        if(m_alignedElementSize != 0)
        {
            AllocPageData(newPage);
        }

        std::memcpy(m_sparse.GetArray(), oldArray, oldSize);

        if(oldArray)
        {
            if(m_allocator)
            {
                m_allocator->Free(oldSize, oldArray);
            }
            else
            {
                std::free(oldArray);
            }
        }

        return newPage;
    }

    SparsePage* SparseSet::CreateOrGetSparsePage(uint32_t pageIndex)
    {
        SparsePage* page = GetSparsePage(pageIndex);

        if(!page)
        {
            page = CreateSparsePage(pageIndex);
        }

        return page;
    }

    void SparseSet::CallocPageDenseIndex(SparsePage* page)
    {
        assert(page && "Page is null");

        if(m_allocator)
        {
            page->denseIndex = CAST(m_allocator->Calloc(sizeof(uint32_t) * SparsePageSize), uint32_t);
        }
        else
        {
            page->denseIndex = CAST(std::calloc(SparsePageSize, sizeof(uint32_t)), uint32_t);
        }
        assert(page->denseIndex && "Page dense index is null!");
    }

    void SparseSet::AllocPageData(SparsePage* page)
    {
        assert(page && "Page is null");

        if(m_pageAllocator)
        {
            page->data = m_pageAllocator->Alloc();
        }
        else
        {
            if(m_allocator)
            {
                page->data = m_allocator->Alloc(m_alignedElementSize * SparsePageSize);
            }
            else
            {
                page->data = std::malloc(m_alignedElementSize * SparsePageSize);
            }
        }
        assert(page->denseIndex && "Page data index is null!");
    }

    void SparseSet::PushBack(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);
        uint32_t pageOffset = GetPageOffset(id);

        SparsePage* page = GetSparsePage(id);

        if(!page)
        {
            page = CreateSparsePage(pageIndex);
        }

        uint32_t denseIndex = page->denseIndex[pageOffset];
        
        if(denseIndex == 0)
        {
            uint32_t denseCount = m_dense.GetCount();
            page->denseIndex[pageOffset] = denseCount;
            
            if(!m_dense.IncreCountCheck())
            {
                void* oldDense = m_dense.GetArray();
                uint32_t oldDenseSize = m_dense.GetAlignedElementSize() * m_dense.GetCapacity();

                m_dense.Grow(m_allocator, m_dense.GetCapacity() + 1);
                
                std::memcpy(m_dense.GetArray(), oldDense, oldDenseSize);

                if(oldDense)
                {
                    m_allocator->Free(oldDenseSize, oldDense);
                }
            }

            CAST(m_dense.GetFirstElement(), uint64_t)[denseCount] = id;
            m_dense.IncreCount();
        }
    }

    bool SparseSet::isValidDense(uint64_t id)
    {
        SparsePage* page = GetSparsePage(GetPageIndex(id));

        if(!page)
        {
            return false;
        }

        uint32_t denseIndex = page->denseIndex[GetPageOffset(id)];

        return denseIndex != 0;
    }

    void* SparseSet::GetSparsePageData(uint64_t id)
    {
        SparsePage* page = GetSparsePage(GetPageIndex(id));
        assert(page && "page is not initialized!");

        if(page->data && m_alignedElementSize != 0)
        {
            return OFFSET_ELEMENT(page->data, m_alignedElementSize, GetPageOffset(id));
        }
        else
        {
            return nullptr;
        }
    }
}