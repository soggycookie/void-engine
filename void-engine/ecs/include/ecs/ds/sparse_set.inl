#include "world_allocator.h"

namespace ECS
{
    template<typename T>
    void SparseSet<T>::Init(WorldAllocator* allocator, BlockAllocator* pageAllocator, 
                         uint32_t elementSize, uint32_t defaultDense)
    {
        m_allocator = allocator;
        m_pageAllocator = pageAllocator;
        m_elementSize = elementSize;

        m_dense.Init(allocator, sizeof(uint64_t), defaultDense);
        m_sparse.Init(allocator, sizeof(SparsePage), 0);

        PTR_CAST(m_dense.PushBack(), uint64_t)[0] = 0;
    }

    template<typename T>
    uint32_t SparseSet<T>::GetPageIndex(uint64_t id)
    {
        return id >> SparsePageBit;
    }

    template<typename T>
    uint32_t SparseSet<T>::GetPageOffset(uint64_t id)
    {
        return id & (SparsePageSize - 1);
    }

    template<typename T>
    SparsePage* SparseSet<T>::GetSparsePage(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);

        if(m_sparse.GetCount() < pageIndex + 1)
        {
            return nullptr;
        }

        return CAST_OFFSET_MEM_ARR_ELEMENT(m_sparse, pageIndex, SparsePage);
    }

    template<typename T>
    SparsePage* SparseSet<T>::CreateSparsePage(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);

        if(m_sparse.GetCount() >= pageIndex + 1)
        {
            return nullptr;
        }

        void* oldArray = m_sparse.GetArray();
        size_t oldSize = m_sparse.GetElementSize() * m_sparse.GetCapacity();

        m_sparse.Grow(m_allocator, pageIndex + 1);

        SparsePage* newPage = PTR_CAST(m_sparse.PushBack(), SparsePage);
        
        assert(newPage && "New page failed to create!");

        CallocPageDenseIndex(newPage);
        
        if(m_elementSize != 0)
        {
            AllocPageData(newPage);
        }


        if(oldArray)
        {
            std::memcpy(m_sparse.GetArray(), oldArray, oldSize);

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

    template<typename T>
    SparsePage* SparseSet<T>::CreateOrGetSparsePage(uint64_t id)
    {
        SparsePage* page = GetSparsePage(id);

        if(!page)
        {
            page = CreateSparsePage(id);
        }

        return page;
    }

    template<typename T>
    void SparseSet<T>::CallocPageDenseIndex(SparsePage* page)
    {
        assert(page && "Page is null");

        if(m_allocator)
        {
            page->denseIndex = PTR_CAST(m_allocator->Calloc(sizeof(uint32_t) * SparsePageSize), uint32_t);
        }
        else
        {
            page->denseIndex = PTR_CAST(std::calloc(SparsePageSize, sizeof(uint32_t)), uint32_t);
        }
        assert(page->denseIndex && "Page dense index is null!");
    }

    template<typename T>
    void SparseSet<T>::AllocPageData(SparsePage* page)
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
                page->data = m_allocator->Alloc(m_elementSize * SparsePageSize);
            }
            else
            {
                page->data = std::malloc(m_elementSize * SparsePageSize);
            }
        }
        assert(page->denseIndex && "Page data index is null!");
    }

    template<typename T>
    void SparseSet<T>::PushBack(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);
        uint32_t pageOffset = GetPageOffset(id);

        SparsePage* page = CreateOrGetSparsePage(id);

        uint32_t denseIndex = page->denseIndex[pageOffset];
        
        if(denseIndex == 0)
        {
            uint32_t denseCount = m_dense.GetCount();
            page->denseIndex[pageOffset] = denseCount;
            
            if(!m_dense.IncreCountCheck())
            {
                void* oldDense = m_dense.GetArray();
                uint32_t oldDenseSize = m_dense.GetElementSize() * m_dense.GetCapacity();

                //NOTE: consider this approach. 
                //Allocator null make dense allocate ineffciently by increasing just 1
                m_dense.Grow(m_allocator, m_dense.GetCapacity() + 1);
                
                std::memcpy(m_dense.GetArray(), oldDense, oldDenseSize);

                if(oldDense)
                {
                    if(m_allocator)
                    {
                        m_allocator->Free(oldDenseSize, oldDense);
                    }
                    else
                    {
                        std::free(oldDense);
                    }
                }
            }

            PTR_CAST(m_dense.GetFirstElement(), uint64_t)[denseCount] = id;
            m_dense.IncreCount();
        }
    }

    template<typename T>
    bool SparseSet<T>::isValidDense(uint64_t id)
    {
        SparsePage* page = GetSparsePage(GetPageIndex(id));

        if(!page)
        {
            return false;
        }

        uint32_t denseIndex = page->denseIndex[GetPageOffset(id)];

        return denseIndex != 0;
    }

    template<typename T>
    void* SparseSet<T>::GetSparsePageData(uint64_t id)
    {
        SparsePage* page = GetSparsePage(GetPageIndex(id));
        assert(page && "page is not initialized!");

        if(page->data && m_elementSize != 0)
        {
            return OFFSET_ELEMENT(page->data, m_elementSize, GetPageOffset(id));
        }
        else
        {
            return nullptr;
        }
    }
}