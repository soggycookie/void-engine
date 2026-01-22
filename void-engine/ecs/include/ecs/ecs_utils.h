#pragma once
#include "ecs_pch.h"

namespace ECS
{

#define KB(x) (1024 * x)
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))

#define OFFSET_ELEMENT(addr, size, count) \
    reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(addr) + size * count))
#define CAST_OFFSET_ELEMENT(addr, T, size, count) \
    reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(addr) + size * count))
#define CAST(addr, T) static_cast<T*>(addr)

#define OFFSET_MEM_ARR_ELEMENT(arr, index) \
    reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(arr.GetArray()) + arr.GetAlignedElementSize() * index))
#define CAST_OFFSET_MEM_ARR_ELEMENT(arr, index, T) \
    reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(arr.GetArray()) + arr.GetAlignedElementSize() * index))

    constexpr size_t DefaultAlignment = alignof(std::max_align_t);

    inline uint32_t Align(uint32_t src, uint32_t alignment)
    {
        uint32_t mask = alignment - 1;
        assert((mask & alignment) == 0 && "Alignment must be power of 2!");

        return (src + mask) & ~mask;
    }

}