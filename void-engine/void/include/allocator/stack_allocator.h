#pragma once
#include "allocator.h"

namespace VoidEngine
{

    class StackAllocator : public Allocator
    {
    public:
        StackAllocator()
            : Allocator(), m_offset(0)
        {
        }

        StackAllocator(size_t totalSize)
            : Allocator(totalSize), m_offset(0), m_prevOffset(0)
        {
            assert(totalSize && "Size of allocation must not be 0! [StackAllocator.Constructor]");
            m_baseAddr = static_cast<uint8_t*>(std::malloc(m_totalSize));

            //TODO: handle fail malloc
            assert(m_baseAddr && "Failed to allocate! [StackAllocator.Constructor]");
        }

        StackAllocator& operator=(StackAllocator&& allocator) noexcept
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
            m_prevOffset = allocator.m_prevOffset;

            allocator.m_baseAddr = nullptr;
            allocator.m_totalSize = 0;    
            allocator.m_offset = 0;
            allocator.m_prevOffset = 0;
        }

        ~StackAllocator()
        {
            if(!m_baseAddr)
            {
                return;
            }
            free(m_baseAddr);
        }

        void* Alloc(size_t size, size_t align) override
        {
            //assert(align >= MIN_SIZE_BYTE);

            size_t mask = align - 1;
            assert((mask & align) == 0); //power of 2

            size_t headerAlign = alignof(Header) - 1;
            size_t headerMask = headerAlign - 1;

            uintptr_t currentAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_offset);
            uintptr_t alignedAddr;

            if(headerAlign > align)
            {
                alignedAddr = (currentAddr + sizeof(Header) + headerMask) & ~headerMask;
            }
            else
            {
                alignedAddr = (currentAddr + sizeof(Header) + mask) & ~mask;
            }

            size_t padding = alignedAddr - currentAddr;

            if((m_offset + padding + size) > m_totalSize)
            {
                assert(0 && "Memory reservation in stack is depleted! [StackAllocator.Alloc]");

                return nullptr;
            }

            m_prevOffset = m_offset;
            m_offset += (padding + size);



            uintptr_t headerAddr = alignedAddr - sizeof(Header);
            Header* header = reinterpret_cast<Header*>(headerAddr);
            header->padding = padding;

            return std::memset(reinterpret_cast<void*>(alignedAddr), 0, size);
        }

        void* Alloc(size_t size) override
        {
            return Alloc(size, DEFAULT_ALIGNMENT);
        }

        void Free(void* addr) override
        {
            if(!addr)
            {
                assert(0 && "Free ptr is null! [StackAllocator.Free]");
                return;
            }

            uintptr_t currAddr = reinterpret_cast<uintptr_t>(addr);
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_baseAddr);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_totalSize);

            if(currAddr < baseAddr || currAddr >= endAddr)
            {
                assert(0 && "Out of bounds memory address passed to stack! [StackAllocator.Free]");
            }

            uintptr_t freeAddr = reinterpret_cast<uintptr_t>(addr);

            Header* header = reinterpret_cast<Header*>(freeAddr - sizeof(Header));

            m_offset = freeAddr - baseAddr - header->padding;
        }

        void Clear() override
        {
            m_offset = 0;
            m_prevOffset = 0;
        }

    private:
        struct Header
        {
            size_t padding;
        };

    private:
        size_t m_offset;
        size_t m_prevOffset;
    };

}