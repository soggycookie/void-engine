#include "world_allocator.h"

namespace ECS
{
    template<typename T>
    void SparseSet<T>::Init(WorldAllocator* allocator, BlockAllocator* pageAllocator, 
                         uint32_t elementSize, uint32_t defaultDense)
    {
        m_allocator = allocator;
        m_pageAllocator = pageAllocator;

        m_dense.Init(allocator, sizeof(uint64_t), defaultDense);
        m_sparse.Init(allocator, sizeof(SparsePage<T>), 0);

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
    SparsePage<T>* SparseSet<T>::GetSparsePage(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);

        if(m_sparse.GetCount() < pageIndex + 1)
        {
            return nullptr;
        }

        return CAST_OFFSET_MEM_ARR_ELEMENT(m_sparse, pageIndex, SparsePage<T>);
    }

    template<typename T>
    SparsePage<T>* SparseSet<T>::CreateSparsePage(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);

        if(m_sparse.GetCount() >= pageIndex + 1)
        {
            return nullptr;
        }

        void* oldArray = m_sparse.GetArray();
        size_t oldSize = m_sparse.GetElementSize() * m_sparse.GetCapacity();

        m_sparse.Grow(m_allocator, pageIndex + 1);

        SparsePage<T>* newPage = PTR_CAST(m_sparse.PushBack(), SparsePage<T>);
        
        assert(newPage && "New page failed to create!");

        CallocPageDenseIndex(newPage);
        AllocPageData(newPage);

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
    SparsePage<T>* SparseSet<T>::CreateOrGetSparsePage(uint64_t id)
    {
        SparsePage<T>* page = GetSparsePage(id);

        if(!page)
        {
            page = CreateSparsePage(id);
        }

        return page;
    }

    template<typename T>
    void SparseSet<T>::CallocPageDenseIndex(SparsePage<T>* page)
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
    void SparseSet<T>::AllocPageData(SparsePage<T>* page)
    {
        assert(page && "Page is null");

        if(m_pageAllocator)
        {
            page->data = PTR_CAST(m_pageAllocator->Alloc(), T);
        }
        else
        {
            if(m_allocator)
            {
                page->data = PTR_CAST(m_allocator->Alloc(sizeof(T) * SparsePageSize), T);
            }
            else
            {
                page->data = PTR_CAST(std::malloc(sizeof(T) * SparsePageSize), T);
            }
        }
    }

    template<typename T>
    void SparseSet<T>::PushBack(uint64_t id)
    {
        uint32_t pageIndex = GetPageIndex(id);
        uint32_t pageOffset = GetPageOffset(id);

        SparsePage<T>* page = CreateOrGetSparsePage(id);

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

            /*
                The dense can have pool of free ID
                Dense must support add new ID, or reused ID
            */


            PTR_CAST(m_dense.GetFirstElement(), uint64_t)[denseCount] = id;
            m_dense.IncreCount();
            ++m_count;
        }
    }

    template<typename T>
    bool SparseSet<T>::isValidDense(uint64_t id)
    {
        SparsePage<T>* page = GetSparsePage(id);

        if(!page)
        {
            return false;
        }

        uint32_t denseIndex = page->denseIndex[GetPageOffset(id)];

        return denseIndex != 0;
    }

    template<typename T>
    bool SparseSet<T>::isValidPage(uint64_t id)
    {
        SparsePage<T>* page = GetSparsePage(id);

        if(!page)
        {
            return false;
        }
        
        return true;
    }

    template<typename T>
    T* SparseSet<T>::GetSparsePageData(uint64_t id)
    {
        SparsePage<T>* page = GetSparsePage(id);
        assert(page && "page is not initialized!");

        if(page->data)
        {
            return CAST_OFFSET_ELEMENT(page->data, T, sizeof(T), GetPageOffset(id));
        }
        else
        {
            return nullptr;
        }
    }

    template<typename T>
    void SparseSet<T>::Remove(uint64_t id)
    {
        if(m_count == 0)
        {
            return;
        }

        SparsePage<T>* page = GetSparsePage(id);

        if(!page)
        {
            return;
        }

        uint32_t denseIndex = page->denseIndex[GetPageOffset(id)];
        
        if(denseIndex == 0)
        {
            return;
        }

        if(m_count > 1){
            //if(m_reservedFreeId)
            //{
            //    SwapDense(denseIndex, m_count);
            //}
            //else
            //{
                SwapDense(denseIndex, m_dense.GetCount() - 1);
                m_dense.DecreCount();
            //}
        }
        else
        {
            //if(m_reservedFreeId)
            //{
            //  
            //}
            //else
            //{
                m_dense.DecreCount();
            //}
        }

        T* data = GetSparsePageData(id);        
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            data->~T();
        }

        page->denseIndex[GetPageOffset(id)] = 0;
        --m_count;
    }

    template<typename T>
    uint32_t SparseSet<T>::GetDenseIndex(uint64_t id)
    {
        SparsePage<T>* page = GetSparsePage(id);

        if(!page)
        {
            return 0;
        }

        uint32_t denseIndex = page->denseIndex[GetPageOffset(id)];
        
        return denseIndex;
    }

    template<typename T>
    void SparseSet<T>::SwapDense(uint32_t srcIndex, uint32_t destIndex)
    {
        assert(destIndex || srcIndex);
        assert(srcIndex < m_dense.GetCount());
        assert(destIndex < m_dense.GetCount());

        uint64_t srcId = *CAST_OFFSET_MEM_ARR_ELEMENT(m_dense, srcIndex, uint64_t);
        uint64_t destId = *CAST_OFFSET_MEM_ARR_ELEMENT(m_dense, destIndex, uint64_t);

        SparsePage<T>* srcPage = GetSparsePage(srcId);
        SparsePage<T>* destPage = GetSparsePage(destId);

        assert(srcPage);
        assert(destPage);

        PTR_CAST(m_dense.GetArray(), uint64_t)[srcIndex] = destId;
        PTR_CAST(m_dense.GetArray(), uint64_t)[destIndex] = srcId;

        srcPage->denseIndex[GetPageOffset(srcId)] = destIndex;
        destPage->denseIndex[GetPageOffset(destId)] = srcIndex;

        T& srcData = srcPage->data[GetPageOffset(srcId)];
        T& destData = destPage->data[GetPageOffset(destId)];

        std::swap(std::move(srcData), std::move(destData));
    }

}