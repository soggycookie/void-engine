#include "allocator.h"
/*
    Serve as a memory provider
*/

namespace ECS
{
    class DynamicArray
    {
    public:
        DynamicArray()
            : m_array(nullptr), m_count(0), m_capacity(0), m_alignedElementSize(0)
        {
        }

        void Init(Allocator* allocator, size_t elementSize, size_t elementAlignment, uint32_t capacity);
        bool IsReqGrow() const;

        bool IncreCount();
        uint32_t GetCount();
        uint32_t GetAlignedElementSize();

        void* GetArray();

        //This function will not grow the capacity
        //We can check ifReqGrow then do the Grow manually
        void* PushBack(Allocator* allocator);

        void* GetBackElement();
        void* GetFirstElement();
        void* GetElement(uint32_t index);

        void* Grow(Allocator* allocator, uint32_t newCapacity);
        void* Alloc(Allocator* allocator);
        void Free(Allocator* allocator);

    private:
        void* m_array;
        uint32_t m_count;
        uint32_t m_capacity;
        uint32_t m_alignedElementSize;
    };

}
