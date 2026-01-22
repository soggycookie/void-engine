#include "ds/memory_array.h"
#include "ds/world_allocator.h"

namespace ECS
{
    void MemoryArray::Init(WorldAllocator* allocator, uint32_t elementSize, uint32_t elementAlignment, uint32_t capacity)
    {
        assert(elementSize && "Elemenet size is 0!");
        m_alignedElementSize = Align(elementSize, elementAlignment);
        m_capacity = capacity;
        m_count = 0;

        m_array = Alloc(allocator);
    }

    bool MemoryArray::IsReqGrow() const
    {
        return m_count == m_capacity;
    }
    
    bool MemoryArray::IncreCountCheck()
    {
        if(m_count + 1 <= m_capacity)
        {
            return true;
        }

        return false;
    }
    
    void MemoryArray::IncreCount()
    {
        ++m_count;
    }

    uint32_t MemoryArray::GetCount()
    {
        return m_count;
    }

    uint32_t MemoryArray::GetCapacity()
    {
        return m_capacity;
    }

    uint32_t MemoryArray::GetAlignedElementSize()
    {
        return m_alignedElementSize;
    }

    void* MemoryArray::GetArray()
    {
        return m_array;
    }


    void* MemoryArray::GetBackElement()
    {
        return OFFSET_ELEMENT(m_array, m_alignedElementSize, m_count - 1);
    }

    void* MemoryArray::GetFirstElement()
    {
        return m_array;
    }

    void* MemoryArray::GetElement(uint32_t index)
    {
        if(index >= m_count)
        {
            return nullptr;
        }

        return OFFSET_ELEMENT(m_array, m_alignedElementSize, index);
    }

    void* MemoryArray::PushBack()
    {
        ++m_count;
        return OFFSET_ELEMENT(m_array, m_alignedElementSize, m_count - 1);
    }

    void MemoryArray::Grow(WorldAllocator* allocator, uint32_t newCapacity)
    {
        if(newCapacity <= m_capacity)
        {
            return;
        }

        m_capacity = newCapacity;
        m_array = Alloc(allocator);
    }


    void* MemoryArray::Alloc(WorldAllocator* allocator)
    {
        size_t size = m_alignedElementSize * m_capacity;
        if(size == 0)
        {
            return nullptr;
        }
        void* data = nullptr;
        if(!allocator)
        {
            data = std::malloc(size);
        }
        else
        {
            data = allocator->Alloc(size);
        }

        assert(data && "Array failed to alloc!");

        return data;
    }

    void MemoryArray::Free(WorldAllocator* allocator)
    {
        if(m_array)
        {
            if(!allocator)
            {
                std::free(m_array);
            }
            else
            {
                allocator->Free(m_alignedElementSize * m_capacity, m_array);
            }
        }
    }

}