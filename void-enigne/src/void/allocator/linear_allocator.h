#pragma once
#include "allocator.h"

namespace VoidEngine
{


    //TODO: Temp block and revert
    class LinearAllocator : public Allocator
    {
    public:
        LinearAllocator(): Allocator(), m_offset(0)
        {
        }

        LinearAllocator(size_t totalSize)
            : Allocator(totalSize), m_offset(0)
        {
            assert(totalSize && "Size of allocation must not be 0! [LinearAllocator.Constructor]");
            m_baseAddr = static_cast<uint8_t*>(std::malloc(m_totalSize));

            assert(m_baseAddr && "Failed to allocate! [LinearAllocator.Constructor]");
        }

        ~LinearAllocator()
        {            
            if(!m_baseAddr)
            {
                return;
            }

            free(m_baseAddr);
        }

        LinearAllocator& operator=(LinearAllocator&& allocator) noexcept
        {
            if(this == &allocator)
            {
                return *this;
            }

            if(m_baseAddr)
            {
                free(m_baseAddr);
            }

            m_baseAddr = allocator.m_baseAddr;
            m_totalSize = allocator.m_totalSize;
            m_offset = allocator.m_offset;

            allocator.m_baseAddr = nullptr;
            allocator.m_totalSize = 0;
            allocator.m_offset = 0;

            return *this;
        }

        void* Alloc(size_t size, size_t align) override
        {
            size_t mask = align - 1;
            assert((mask & align) == 0); //power of 2

            uintptr_t currentAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_offset);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_totalSize);

            uintptr_t alignedAddr = (currentAddr + mask) & ~mask;

            if(alignedAddr >= endAddr)
            {
                assert(0 && "The linear allocator reservation is depleted! [LinearAllocator.Alloc]");
                return nullptr;
            }

            m_offset += (alignedAddr - currentAddr + size);

            return std::memset(reinterpret_cast<void*>(alignedAddr), 0, size);
        }

        void* Alloc(size_t size) override
        {
            return Alloc(size, DEFAULT_ALIGNMENT);
        }

        void Free(void* addr) override
        {
        }

        void Clear() override
        {
            m_offset = 0;
        }

        //size_t UsedSize() const
        //{
        //    return m_offset;
        //}

    private:
        size_t m_offset;
    };

}