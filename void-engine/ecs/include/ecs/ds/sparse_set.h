#pragma once
#include "memory_array.h"

namespace ECS
{
    class WorldAllocator;
    class BlockAllocator;

    constexpr uint32_t SparsePageBit = 6;
    constexpr uint32_t SparsePageSize = 1 << SparsePageBit;

    template<typename T>
    struct SparsePage
    {
        uint32_t* denseIndex;
        T* data;
    };

    template<typename T>
    class SparseSet
    {
    public:
        SparseSet()
            : m_dense(), m_sparse(), m_count(0),
            m_allocator(nullptr), m_pageAllocator(nullptr)
        {
            static_assert(std::is_constructible_v<T>);
            static_assert(
                std::is_copy_constructible_v<T> ||
                std::is_move_constructible_v<T>
            );

            static_assert(
                std::is_copy_assignable_v<T> ||
                std::is_move_assignable_v<T>
            );

            static_assert(std::is_destructible_v<T>);
        }

        void Init(WorldAllocator* allocator, BlockAllocator* pageAllocator, 
                  uint32_t elementSize, uint32_t defaultDense);
        

        bool isValidDense(uint64_t id);
        bool isValidPage(uint64_t id);

        T* GetSparsePageData(uint64_t id);
        uint32_t GetDenseIndex(uint64_t id);

        void SwapDense(uint32_t srcIndex, uint32_t destIndex);

        uint32_t GetPageIndex(uint64_t id);
        uint32_t GetPageOffset(uint64_t id);
        
        //this will grow dense and sparse if needed
        void PushBack(uint64_t id);

        void CallocPageDenseIndex(SparsePage<T>* page);
        void AllocPageData(SparsePage<T>* page);

        void Remove(uint64_t id);

        SparsePage<T>* GetSparsePage(uint64_t id);
        SparsePage<T>* CreateSparsePage(uint64_t id);
        SparsePage<T>* CreateOrGetSparsePage(uint64_t id);

        void Destroy();

    private:
        MemoryArray m_dense;
        MemoryArray m_sparse;
        WorldAllocator* m_allocator;
        BlockAllocator* m_pageAllocator;
        uint32_t m_count;
    };

}

/*
    Template function definition
*/
#include "sparse_set.inl"

