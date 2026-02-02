#pragma once
#include "memory_array.h"

namespace ECS
{
    class WorldAllocator;
    class BlockAllocator;

    constexpr uint32_t SparsePageBit = 6;
    constexpr uint32_t SparsePageCount = 1 << SparsePageBit;

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
            : m_dense(), m_sparse(), m_count(0), m_reservedFreeId(false),
            m_allocator(nullptr), m_pageAllocator(nullptr)
        {
            static_assert(
                std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                "T must be copy-constructible or move-constructible"
                );

            static_assert(
                std::is_copy_assignable_v<T> || std::is_move_assignable_v<T>,
                "T must be copy-assignable or move-assignable"
                );

            static_assert(std::is_destructible_v<T>);
        }

        void Init(WorldAllocator* allocator, BlockAllocator* pageAllocator, 
                  uint32_t defaultDense, bool reservedFreeId);
        
        uint32_t GetCount() const
        {
            return m_count;
        }

        bool isValidDense(uint64_t id);
        bool isValidPage(uint64_t id);

        uint64_t* GetDenseArr()
        {
            return PTR_CAST(m_dense.GetArray(), uint64_t);
        }

        T* GetPageData(uint64_t id);
        uint32_t GetDenseIndex(uint64_t id);

        void SwapDense(uint32_t srcIndex, uint32_t destIndex, bool swapPageDense);

        uint32_t GetPageIndex(uint32_t id);
        uint32_t GetPageOffset(uint32_t id);
        
        //this will grow dense and sparse if needed
        template<typename U>
        uint32_t PushBack(uint64_t id, U&& element, bool newId = true);

        void AllocPage(SparsePage<T>* page);
        void CallocPageDenseIndex(SparsePage<T>* page);
        void AllocPageData(SparsePage<T>* page);

        void Remove(uint64_t id);

        SparsePage<T>* GetSparsePage(uint64_t id);
        SparsePage<T>* CreateSparsePage(uint64_t id);
        SparsePage<T>* CreateOrGetSparsePage(uint64_t id);

        void Destroy();

        void PrintAllDense();
        void PrintAliveDense();
        void PrintDeadDense();

        uint64_t GetReservedFreeId();

    private:
        MemoryArray m_dense;
        MemoryArray m_sparse;
        WorldAllocator* m_allocator;
        BlockAllocator* m_pageAllocator;
        uint32_t m_count; //Alive id
        bool m_reservedFreeId;
    };

}

