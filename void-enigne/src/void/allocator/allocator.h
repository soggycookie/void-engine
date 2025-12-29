#pragma once
#include "void/pch.h"

namespace VoidEngine
{

    class Allocator
    {
    public:
        Allocator()
            :m_baseAddr(0), m_totalSize(0)
        {
        }

        Allocator(size_t totalSize)
            : m_totalSize(totalSize), m_baseAddr(nullptr)
        {
        }

        Allocator& operator=(Allocator&& allocator) noexcept = delete;
        Allocator(Allocator&&) noexcept = delete;

        virtual ~Allocator() = default;

        virtual void* Alloc(size_t size, size_t align) = 0;
        virtual void* Alloc(size_t size) = 0;
        virtual void Free(void* addr) = 0;
        virtual void Clear() = 0;

        virtual Allocator& operator=(const Allocator& allocator) = default;

    protected:
        uint8_t* m_baseAddr;
        size_t m_totalSize;
    };

}