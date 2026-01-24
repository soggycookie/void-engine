#pragma once
#include "memory_array.h"

namespace ECS
{
    class WorldAllocator;
    class BlockAllocator;

    constexpr uint32_t SparsePageBit = 6;
    constexpr uint32_t SparsePageSize = 1 << SparsePageBit;

    struct SparsePage
    {
        uint32_t* denseIndex;
        void* data;
    };

    class SparseSet
    {
    public:
        SparseSet()
            : m_dense(), m_sparse(), m_count(0), m_elementSize(0),
            m_allocator(nullptr), m_pageAllocator(nullptr)
        {
        }

        void Init(WorldAllocator* allocator, BlockAllocator* pageAllocator, 
                  uint32_t elementSize, uint32_t defaultDense);
        
        //this will grow dense and sparse if needed
        void PushBack(uint64_t id);

        bool isValidDense(uint64_t id);

        void* GetSparsePageData(uint64_t id);

        void CallocPageDenseIndex(SparsePage* page);
        void AllocPageData(SparsePage* page);

        uint32_t GetPageIndex(uint64_t id);
        uint32_t GetPageOffset(uint64_t id);

        SparsePage* GetSparsePage(uint64_t id);
        SparsePage* CreateSparsePage(uint64_t id);
        SparsePage* CreateOrGetSparsePage(uint64_t id);

        void Destroy();

    private:
        MemoryArray m_dense;
        MemoryArray m_sparse;
        WorldAllocator* m_allocator;
        BlockAllocator* m_pageAllocator;
        uint32_t m_count;
        uint32_t m_elementSize;
    };
}

