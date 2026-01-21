#pragma once
#include "allocator.h"

namespace VoidEngine
{

    class PoolAllocator : public Allocator
    {
    public:
        PoolAllocator() : 
            Allocator(), m_chunkSize(0), m_alignedChunkSize(0),
            m_chunkAlignment(0), m_head(nullptr), m_startAddr(nullptr),
            m_chunkCount(0)
        {
        }

        PoolAllocator(size_t totalSize, size_t chunkSize, size_t chunkAlignment)
            : Allocator(totalSize), m_chunkSize(chunkSize), m_alignedChunkSize(0),
            m_chunkAlignment(chunkAlignment), m_head(nullptr)
        {
            assert(totalSize && "Size of allocation must not be 0! [PoolAllcator.Constructor]");
            assert(totalSize >= chunkSize && "Size of allocation must be larger than chunk size! [PoolAllcator.Constructor]");

            size_t headerAlign = alignof(FreePool) - 1;
            size_t headerMask = headerAlign - 1;

            size_t mask = m_chunkAlignment - 1;
            assert((mask & m_chunkAlignment) == 0 && "Alignment must be power of 2! [PoolAllocator.Constructor]"); //power of 2

            m_baseAddr = static_cast<uint8_t*>(std::malloc(m_totalSize));
            //TODO: handle fail malloc
            assert(m_baseAddr && "Failed to allocate! [PoolAllcator.Constructor]");

            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_baseAddr);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_totalSize);

            uintptr_t alignedAddr;

            if(headerAlign > chunkAlignment)
            {
                alignedAddr = (baseAddr + sizeof(FreePool) + headerMask) & ~headerMask;
                m_alignedChunkSize = (chunkSize + headerMask) & ~headerMask;
            }
            else
            {
                alignedAddr = (baseAddr + sizeof(FreePool) + mask) & ~mask;
                m_alignedChunkSize = (chunkSize + mask) & ~mask;
            }

            m_startAddr = reinterpret_cast<uint8_t*>(alignedAddr);

            //m_chunkSize = m_chunkAlignment;
            assert(m_chunkSize >= sizeof(FreePool) && "Chunk is too small! [PoolAllocator.Constructor]");

            m_chunkCount = (endAddr - alignedAddr) / m_alignedChunkSize;

            Clear();
        }

        ~PoolAllocator()
        {
            if(!m_baseAddr)
            {
                return;
            }

            free(m_baseAddr);
        }

        PoolAllocator& operator=(PoolAllocator&& allocator) noexcept
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
            m_startAddr = allocator.m_startAddr;
            m_head = allocator.m_head;
            m_chunkSize = allocator.m_chunkSize;
            m_alignedChunkSize = allocator.m_alignedChunkSize;
            m_chunkCount = allocator.m_chunkCount;
            m_chunkAlignment = allocator.m_chunkAlignment;

            allocator.m_baseAddr = nullptr;
            allocator.m_totalSize = 0;
            allocator.m_startAddr = nullptr;
            allocator.m_head = nullptr;
            allocator.m_chunkSize = 0;
            allocator.m_alignedChunkSize = 0;
            allocator.m_chunkCount = 0;
            allocator.m_chunkAlignment = 0;

            return *this;            
        }

        void* Alloc(size_t size, size_t align) override
        {
            FreePool* pool = m_head;

            if(!pool)
            {
                assert(pool && "Pool reservation is depleted! [PoolAllocator.Alloc]");
                return nullptr;
            }

            m_head = pool->next;

            return std::memset(pool, 0, m_chunkSize);
        }

        void* Alloc(size_t size) override
        {
            return Alloc(size, DEFAULT_ALIGNMENT);
        }

        void Free(void* addr) override
        {
            if(addr == nullptr)
            {
                assert(0 && "Free ptr is NULL [PoolAllocator.Free]");
            }

            uintptr_t curAddr = reinterpret_cast<uintptr_t>(addr);
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_baseAddr);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_chunkCount * m_chunkSize);

            if(curAddr < baseAddr || curAddr >= endAddr)
            {
                assert(0 && "Out of bounds memory address passed to pool [PoolAllocator.Free]");
                return;
            }

            FreePool* pool = static_cast<FreePool*>(addr);
            pool->next = m_head;
            m_head = pool;
        }

        void Clear() override
        {
            for(size_t i = 0; i < m_chunkCount; i++)
            {
                FreePool* pool = reinterpret_cast<FreePool*>(m_startAddr + i * m_alignedChunkSize);
                pool->next = m_head;
                m_head = pool;
            }
        }

    private:
        struct FreePool
        {
            FreePool* next;
        };

    private:
        uint8_t* m_startAddr;
        FreePool* m_head;
        size_t m_chunkSize;
        size_t m_alignedChunkSize;
        size_t m_chunkCount;
        size_t m_chunkAlignment;
    };

}