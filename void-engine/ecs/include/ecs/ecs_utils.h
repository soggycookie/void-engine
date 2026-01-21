#pragma once
#include "ecs_pch.h"

namespace ECS
{

#define KB(x) (1024 * x)
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))
#define OFFSET_ELEMENT(addr, size, count) reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(addr) + size * count))
#define CAST_OFFSET_ELEMENT(addr, T, size, count) reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(addr) + size * count))


    constexpr size_t DefaultAlignment = alignof(std::max_align_t);

    inline size_t Align(size_t src, size_t alignment)
    {
        size_t mask = alignment - 1;
        assert((mask & alignment) == 0 && "Alignment must be power of 2!");

        return (src + mask) & ~mask;
    }

}