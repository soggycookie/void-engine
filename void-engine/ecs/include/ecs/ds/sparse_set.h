#pragma once
#include "allocator.h"
#include "dynamic_array.h"

namespace ECS
{
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
            : m_dense(), m_sparse(), m_count(0),
            m_allocator(nullptr), m_pageAllocator(nullptr)
        {
        }

        void Init(Allocator* allocator, BlockAllocator* pageAllocator);
        void PushBackDense();
        void PushBack(uint64_t id);
        uint32_t GetPageIndex(uint64_t id);
        uint32_t GetPageOffset(uint64_t id);

        SparsePage* GetSparsePage(uint64_t id);
        SparsePage* CreateSparsePage(uint32_t pageIndex);

    private:
        DynamicArray m_dense;
        DynamicArray m_sparse;
        Allocator* m_allocator;
        BlockAllocator* m_pageAllocator;
        uint32_t m_count;
    };
}

