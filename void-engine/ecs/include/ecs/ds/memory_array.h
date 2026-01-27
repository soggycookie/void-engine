#pragma once
/*
    Serve as a memory provider
*/

namespace ECS
{
    class WorldAllocator;

    class MemoryArray
    {
    public:
        MemoryArray()
            : m_array(nullptr), m_count(0), m_capacity(0), m_elementSize(0)
        {
        }

        void Init(WorldAllocator* allocator, uint32_t elementSize, uint32_t capacity);
        bool IsReqGrow() const;

        bool IncreCountCheck();
        void IncreCount();
        void DecreCount();
        
        uint32_t GetCount();
        uint32_t GetCapacity();
        uint32_t GetElementSize();

        void* GetArray();

        //This function will not grow the capacity
        //We can check ifReqGrow then do the Grow manually
        void* PushBack();

        void* GetBackElement();
        void* GetFirstElement();
        void* GetElement(uint32_t index);

        void Grow(WorldAllocator* allocator, uint32_t newCapacity);
        void* Alloc(WorldAllocator* allocator);
        void Free(WorldAllocator* allocator);

    private:
        void* m_array;
        uint32_t m_count;
        uint32_t m_capacity;
        uint32_t m_elementSize;
    };

}
