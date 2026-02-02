
namespace ECS
{
    template<typename T>
    void SparseSet<T>::Init(WorldAllocator* allocator, BlockAllocator* pageAllocator, 
                            uint32_t defaultDense,  bool reservedFreeId)
    {
        m_allocator = allocator;
        m_pageAllocator = pageAllocator;
        m_reservedFreeId = reservedFreeId;

        defaultDense = defaultDense == 0 ? 1 : defaultDense;

        m_dense.Init(allocator, sizeof(uint64_t), defaultDense);
        m_sparse.Init(allocator, sizeof(SparsePage<T>), 0);

        PTR_CAST(m_dense.GetFirstElement(), uint64_t)[0] = 0;
        m_dense.IncreCount();
    }

    template<typename T>
    uint32_t SparseSet<T>::GetPageIndex(uint32_t id)
    {
        return id >> SparsePageBit;
    }

    template<typename T>
    uint32_t SparseSet<T>::GetPageOffset(uint32_t id)
    {
        return id & (SparsePageCount - 1);
    }

    template<typename T>
    SparsePage<T>* SparseSet<T>::GetSparsePage(uint64_t id)
    {
        uint32_t lowId = CAST(id, uint32_t);

        uint32_t pageIndex = GetPageIndex(lowId);

        if(m_sparse.GetCount() < pageIndex + 1)
        {
            return nullptr;
        }

        auto page = CAST_OFFSET_MEM_ARR_ELEMENT(m_sparse, pageIndex, SparsePage<T>);

        if(!page->denseIndex && !page->data)
        {
            AllocPage(page);
        }

        return page;
    }

    template<typename T>
    SparsePage<T>* SparseSet<T>::CreateSparsePage(uint64_t id)
    {
        uint32_t lowId = CAST(id, uint32_t);

        uint32_t pageIndex = GetPageIndex(lowId);

        if(m_sparse.GetCount() >= pageIndex + 1)
        {
            return nullptr;
        }

        bool reqGrow = (pageIndex + 1) >  m_sparse.GetCapacity();

        void* oldArray = m_sparse.GetArray();
        size_t oldSize = m_sparse.GetElementSize() * m_sparse.GetCapacity();
        m_sparse.Grow(m_allocator, pageIndex + 1);
        
        m_sparse.AddCount((pageIndex + 1) - m_sparse.GetCount());
        SparsePage<T>* newPage = PTR_CAST(m_sparse.GetElement(pageIndex), SparsePage<T>);
        
        assert(newPage && "New page failed to create!");

        AllocPage(newPage);

        if(oldArray && reqGrow)
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
    void SparseSet<T>::AllocPage(SparsePage<T>* page)
    {
        CallocPageDenseIndex(page);
        AllocPageData(page);
    }

    template<typename T>
    void SparseSet<T>::CallocPageDenseIndex(SparsePage<T>* page)
    {
        assert(page && "Page is null");

        if(m_allocator)
        {
            page->denseIndex = PTR_CAST(m_allocator->Calloc(sizeof(uint32_t) * SparsePageCount), uint32_t);
        }
        else
        {
            page->denseIndex = PTR_CAST(std::calloc(SparsePageCount, sizeof(uint32_t)), uint32_t);
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
                page->data = PTR_CAST(m_allocator->Alloc(sizeof(T) * SparsePageCount), T);
            }
            else
            {
                page->data = PTR_CAST(std::malloc(sizeof(T) * SparsePageCount), T);
            }
        }
    }

    template<typename T>
    template<typename U>
    uint32_t SparseSet<T>::PushBack(uint64_t id, U&& element, bool newId)
    {
        uint32_t lowId = CAST(id, uint32_t);

        uint32_t pageIndex = GetPageIndex(lowId);
        uint32_t pageOffset = GetPageOffset(lowId);

        SparsePage<T>* page = CreateOrGetSparsePage(lowId);

        uint32_t denseIndex = page->denseIndex[pageOffset];
        
        if(denseIndex == 0)
        {
            uint64_t freeId = 0;
            uint32_t nextAliveCount = m_count + 1;
            uint32_t denseCount = m_dense.GetCount();
           
            if(m_dense.IsReqGrow())
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

            if(newId)
            {
                if(denseCount > nextAliveCount)
                {
                    PTR_CAST(m_dense.GetFirstElement(), uint64_t)[denseCount] = id;
                    m_dense.IncreCount();
                    SwapDense(denseCount, nextAliveCount, false);
                }
                else
                {
                    PTR_CAST(m_dense.GetFirstElement(), uint64_t)[nextAliveCount] = id;
                    m_dense.IncreCount();
                
                }
            }

            page->denseIndex[pageOffset] = nextAliveCount;
            new (GetPageData(lowId)) T(std::move(element));

            ++m_count;

            return nextAliveCount;
        }
        else
        {
            std::cout << "Id exist!" << std::endl;
            return 0;
        }
    }

    template<typename T>
    bool SparseSet<T>::isValidDense(uint64_t id)
    {
        uint32_t lowId = CAST(id, uint32_t);

        SparsePage<T>* page = GetSparsePage(lowId);

        if(!page)
        {
            return false;
        }

        uint32_t denseIndex = page->denseIndex[GetPageOffset(lowId)];

        return denseIndex != 0;
    }

    template<typename T>
    bool SparseSet<T>::isValidPage(uint64_t id)
    {
        uint32_t lowId = CAST(id, uint32_t);

        SparsePage<T>* page = GetSparsePage(lowId);

        if(!page)
        {
            return false;
        }
        
        return true;
    }

    template<typename T>
    T* SparseSet<T>::GetPageData(uint64_t id)
    {
        uint32_t lowId = CAST(id, uint32_t);

        SparsePage<T>* page = GetSparsePage(lowId);
        assert(page && "page is not initialized!");

        uint32_t denseIndex = page->denseIndex[GetPageOffset(lowId)];

        if(page->data && denseIndex != 0)
        {
            return CAST_OFFSET_ELEMENT(page->data, T, sizeof(T), GetPageOffset(lowId));
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

        uint32_t lowId = CAST(id, uint32_t);
        SparsePage<T>* page = GetSparsePage(lowId);

        if(!page)
        {
            return;
        }

        uint32_t denseIndex = page->denseIndex[GetPageOffset(lowId)];
        
        if(denseIndex == 0)
        {
            return;
        }

        if(m_count > 1){
            if(m_reservedFreeId)
            {
                if(denseIndex != m_count)
                {
                    SwapDense(denseIndex, m_count, true);
                }
            }
            else
            {
                if(denseIndex != m_dense.GetCount() - 1)
                {
                    SwapDense(denseIndex, m_dense.GetCount() - 1, true);
                }
                m_dense.DecreCount();
            }
        }
        else
        {
            if(m_reservedFreeId)
            {
            }
            else
            {
                m_dense.DecreCount();
            }
        }

        T* data = GetPageData(lowId);        
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            data->~T();
        }

        page->denseIndex[GetPageOffset(lowId)] = 0;
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

    /// <summary>
    /// Swap Page Dense is for reused id flow
    /// where the page dense is set correctly before
    /// it just has to swap the dense array to work
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <param name="srcIndex"></param>
    /// <param name="destIndex"></param>
    /// <param name="swapPageDense"></param>
    template<typename T>
    void SparseSet<T>::SwapDense(uint32_t srcIndex, uint32_t destIndex, bool swapPageDense)
    {
        assert(destIndex || srcIndex);
        assert(srcIndex < m_dense.GetCount());
        assert(destIndex < m_dense.GetCount());

        uint64_t srcId = *CAST_OFFSET_MEM_ARR_ELEMENT(m_dense, srcIndex, uint64_t);
        uint64_t destId = *CAST_OFFSET_MEM_ARR_ELEMENT(m_dense, destIndex, uint64_t);

        PTR_CAST(m_dense.GetArray(), uint64_t)[srcIndex] = destId;
        PTR_CAST(m_dense.GetArray(), uint64_t)[destIndex] = srcId;

        if(swapPageDense)
        {
            SparsePage<T>* srcPage = GetSparsePage(srcId);
            SparsePage<T>* destPage = GetSparsePage(destId);

            assert(srcPage);
            assert(destPage);


            srcPage->denseIndex[GetPageOffset(srcId)] = destIndex;
            destPage->denseIndex[GetPageOffset(destId)] = srcIndex;
        }
    }

    template<typename T>
    void SparseSet<T>::PrintAllDense()
    {
        for(uint32_t i = 0; i < m_dense.GetCount(); i++)
        {
            std::cout << PTR_CAST(m_dense.GetArray(), uint64_t)[i] << ", ";
        }
        std::cout << std::endl;
    }

    template<typename T>
    void SparseSet<T>::PrintAliveDense()
    {
        for(uint32_t i = 0; i <= m_count; i++)
        {
            std::cout << PTR_CAST(m_dense.GetArray(), uint64_t)[i] << ", ";
        }
        std::cout << std::endl;
    }

    template<typename T>
    void SparseSet<T>::PrintDeadDense()
    {
        for(uint32_t i = m_count + 1; i < m_dense.GetCount(); i++)
        {
            std::cout << PTR_CAST(m_dense.GetArray(), uint64_t)[i] << ", ";
        }
        std::cout << std::endl;
    }

    template<typename T>
    uint64_t SparseSet<T>::GetReservedFreeId()
    {
        if(!m_reservedFreeId)
        {
            return 0;
        }

        if(m_dense.GetCount() <= (m_count + 1))
        {
            return 0;
        }

        uint64_t id = *CAST_OFFSET_ELEMENT(m_dense.GetArray(), uint64_t, 
                                          m_dense.GetElementSize(), m_count + 1);

        return id;
    }

    template<typename T>
    void SparseSet<T>::Destroy()
    {
        for(uint32_t i = 1; i <= m_count; i++)
        {
            uint64_t id = *CAST_OFFSET_ELEMENT(m_dense.GetArray(), uint64_t,
                                               m_dense.GetElementSize(), i);

            T* element = GetPageData(id);
            
            assert(element);
            
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                element->~T();
            }
        }

        for(uint32_t i = 0; i < m_sparse.GetCount(); i++)
        {
            SparsePage<T>* page = CAST_OFFSET_MEM_ARR_ELEMENT(m_sparse, i, SparsePage<T>);
            
            assert(page);

            if(m_allocator)
            {
                m_allocator->Free(SparsePageCount * sizeof(uint32_t), page->denseIndex);
            }
            else
            {
                std::free(page->denseIndex);
            }

            if(m_pageAllocator)
            {
                m_pageAllocator->Free(page->data);
            }
            else
            {
                if(m_allocator)
                {
                    m_allocator->Free(SparsePageCount * sizeof(T), page->data);
                }
                else
                {
                    std::free(page->data);
                }            
            }
        }

        m_dense.Free(m_allocator);
        m_sparse.Free(m_allocator);
        m_count = 0;
    }

}