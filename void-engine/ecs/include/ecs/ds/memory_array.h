#pragma once
/*
    @Serve as a type erasure memory array
    @To construct/destruct, grow,... elements, you must manually operate on it's
   parent layer
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

    void Init(WorldAllocator *allocator, uint32_t elementSize,
              uint32_t capacity);

    bool IsReqGrow() const;

    void IncreCount();
    void AddCount(uint32_t amount);
    void DecreCount();

    uint32_t GetCount();
    uint32_t GetCapacity();
    uint32_t GetElementSize();

    void *GetArray();

    void *GetBackElement();
    void *GetFirstElement();
    void *GetElement(uint32_t index);

    // This will not free/release old memory,
    // because underlying type is unknown, and it might need to be explicitly
    // moved/copied/destructed To grow, call IsReqGrow, get old array, then call
    // this func After that move/copy the data into new array, then free the old
    // one
    void Grow(WorldAllocator *allocator, uint32_t newCapacity);
    void CGrow(WorldAllocator *allocator, uint32_t newCapacity);
    void *AllocN(WorldAllocator *allocator, uint32_t &expandedCapacity);
    void *CallocN(WorldAllocator *allocator, uint32_t &expandedCapacity);
    void Free(WorldAllocator *allocator);

private:
    void *m_array;
    uint32_t m_count;
    uint32_t m_capacity;
    uint32_t m_elementSize;
};

} // namespace ECS
