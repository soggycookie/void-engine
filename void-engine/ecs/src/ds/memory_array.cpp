#include "ds/memory_array.h"
#include "ds/world_allocator.h"

namespace ECS
{
void MemoryArray::Init(WorldAllocator *allocator, uint32_t elementSize,
                       uint32_t capacity)
{
    assert(elementSize && "Elemenet size is 0!");
    m_elementSize = elementSize;
    m_count = 0;

    m_array = CallocN(allocator, capacity);
    m_capacity = capacity;
}

bool MemoryArray::IsReqGrow() const { return m_count == m_capacity; }

void MemoryArray::IncreCount() { ++m_count; }

void MemoryArray::AddCount(uint32_t amount) { m_count += amount; }

void MemoryArray::DecreCount()
{
    if (m_count == 0)
    {
        return;
    }

    --m_count;
}

uint32_t MemoryArray::GetCount() { return m_count; }

uint32_t MemoryArray::GetCapacity() { return m_capacity; }

uint32_t MemoryArray::GetElementSize() { return m_elementSize; }

void *MemoryArray::GetArray() { return m_array; }

void *MemoryArray::GetBackElement()
{
    return OFFSET_ELEMENT(m_array, m_elementSize, m_count - 1);
}

void *MemoryArray::GetFirstElement() { return m_array; }

void *MemoryArray::GetElement(uint32_t index)
{
    if (index >= m_count)
    {
        return nullptr;
    }

    return OFFSET_ELEMENT(m_array, m_elementSize, index);
}

void MemoryArray::Grow(WorldAllocator *allocator, uint32_t newCapacity)
{
    if (newCapacity <= m_capacity)
    {
        return;
    }

    uint32_t expandedCapacity = newCapacity;
    m_array = AllocN(allocator, expandedCapacity);
    m_capacity = expandedCapacity;
}

void MemoryArray::CGrow(WorldAllocator *allocator, uint32_t newCapacity)
{
    if (newCapacity <= m_capacity)
    {
        return;
    }

    uint32_t expandedCapacity = newCapacity;
    m_array = CallocN(allocator, expandedCapacity);
    m_capacity = expandedCapacity;
}

void *MemoryArray::AllocN(WorldAllocator *allocator, uint32_t &expandedCapacity)
{
    uint32_t newCapacity = expandedCapacity;

    size_t size = m_elementSize * newCapacity;
    if (size == 0)
    {
        return nullptr;
    }

    void *data = nullptr;

    if (!allocator)
    {
        data = std::malloc(size);
    }
    else
    {
        data = allocator->AllocN(m_elementSize, newCapacity, expandedCapacity);
    }

    assert(data && "Array failed to alloc!");

    return data;
}

void *MemoryArray::CallocN(WorldAllocator *allocator,
                           uint32_t &expandedCapacity)
{
    uint32_t newCapacity = expandedCapacity;

    size_t size = m_elementSize * newCapacity;
    if (size == 0)
    {
        return nullptr;
    }

    void *data = nullptr;

    if (!allocator)
    {
        data = std::calloc(1, size);
    }
    else
    {
        data = allocator->CallocN(m_elementSize, newCapacity, expandedCapacity);
    }

    assert(data && "Array failed to alloc!");

    return data;
}

void MemoryArray::Free(WorldAllocator *allocator)
{
    if (m_array)
    {
        if (!allocator)
        {
            std::free(m_array);
        }
        else
        {
            allocator->Free(m_elementSize * m_capacity, m_array);
        }
        m_array = nullptr;
        m_count = 0;
        m_capacity = 0;
        m_elementSize = 0;
    }
}

} // namespace ECS
