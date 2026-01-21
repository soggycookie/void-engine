#pragma once
#include "pch.h"
#include "allocator/linear_allocator.h"

namespace VoidEngine
{
    class GlobalPersistantAllocator
    {
    private:
        GlobalPersistantAllocator(size_t totalSize)
            : m_linearAllocator(totalSize)
        {
        }

    public:
        GlobalPersistantAllocator(const GlobalPersistantAllocator&) = delete;
        GlobalPersistantAllocator& operator=(const GlobalPersistantAllocator&) = delete;
        
        static void SetBufferSize(size_t bufferSize)
        {
            //TODO:handle user resize bufferSize 
            m_bufferSize = bufferSize;
        }

        static GlobalPersistantAllocator& Get()
        {
            static GlobalPersistantAllocator instance(m_bufferSize);
            return instance;
        }

        void* Alloc(size_t size, size_t align);
        void* Alloc(size_t size);
        
        void  Clear();

    private:
        static size_t m_bufferSize;
        LinearAllocator m_linearAllocator;
    };
}