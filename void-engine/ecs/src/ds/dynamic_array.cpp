#include "ds/dynamic_array.h"

namespace ECS
{
    void DynamicArray::Init(Allocator* allocator, size_t elementSize, size_t elementAlignment, uint32_t capacity)
    {
        assert(elementSize && "Elemenet size is 0!");
        m_alignedElementSize = Align(elementSize, elementAlignment);
        m_capacity = capacity;
        m_count = 0;
        m_array = Alloc(allocator);
    }

    bool DynamicArray::IsReqGrow() const
    {
        return m_count == m_capacity;
    }
    
    bool DynamicArray::IncreCount()
    {
        if(m_count + 1 <= m_capacity)
        {
            ++m_count;
            return true;
        }

        return false;
    }

    uint32_t DynamicArray::GetCount()
    {
        return m_count;
    }
    uint32_t DynamicArray::GetAlignedElementSize()
    {
        return m_alignedElementSize;
    }

    void* DynamicArray::GetArray()
    {
        return m_array;
    }


    void* DynamicArray::GetBackElement()
    {
        return OFFSET_ELEMENT(m_array, m_alignedElementSize, m_count - 1);
    }

    void* DynamicArray::GetFirstElement()
    {
        return m_array;
    }

    void* DynamicArray::GetElement(uint32_t index)
    {
        if(index >= m_count)
        {
            return nullptr;
        }

        return OFFSET_ELEMENT(m_array, m_alignedElementSize, index);
    }

    void* DynamicArray::PushBack(Allocator* allocator)
    {
        m_count++;
        return OFFSET_ELEMENT(m_array, m_alignedElementSize, m_count - 1);
    }

    void* DynamicArray::Grow(Allocator* allocator, uint32_t newCapacity)
    {
        if(newCapacity <= m_capacity)
        {
            return nullptr;
        }

        m_capacity = newCapacity;
        return Alloc(allocator);
    }


    void* DynamicArray::Alloc(Allocator* allocator)
    {
        size_t size = m_alignedElementSize * m_capacity;
        if(size == 0)
        {
            return nullptr;
        }

        if(!allocator)
        {
            return std::malloc(size);
        }
        else
        {
            return allocator->Alloc(size);
        }
    }

    void DynamicArray::Free(Allocator* allocator)
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