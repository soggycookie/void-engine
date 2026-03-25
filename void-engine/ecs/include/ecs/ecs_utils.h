#pragma once
#include "ecs_pch.h"

namespace ECS
{

#define KB(x) (1024 * x)
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))

#define OFFSET_ELEMENT(addr, size, index)                                      \
    reinterpret_cast<void *>(                                                  \
        (reinterpret_cast<uintptr_t>(addr) + (size) * (index)))
#define CAST_OFFSET_ELEMENT(addr, T, size, index)                              \
    reinterpret_cast<T *>(                                                     \
        (reinterpret_cast<uintptr_t>(addr) + (size) * (index)))
#define PTR_CAST(addr, T) static_cast<T *>(addr)
#define PTR_RCAST(addr, T) reinterpret_cast<T *>(addr)
#define CAST(v, T) static_cast<T>(v)
#define RCAST(v, T) reinterpret_cast<T>(v)

#define OFFSET_MEM_ARR_ELEMENT(arr, index)                                     \
    reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(arr.GetArray()) +    \
                              arr.GetElementSize() * index))
#define CAST_OFFSET_MEM_ARR_ELEMENT(arr, index, T)                             \
    reinterpret_cast<T *>((reinterpret_cast<uintptr_t>(arr.GetArray()) +       \
                           arr.GetElementSize() * index))

#define OFFSET(addr, size)                                                     \
    reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(addr) + (size)))

constexpr size_t DefaultAlignment = alignof(std::max_align_t);

inline uint32_t Align(uint32_t n, uint32_t alignment)
{
    uint32_t mask = alignment - 1;
    assert((mask & alignment) == 0 && "Alignment must be power of 2!");

    return (n + mask) & ~mask;
}

inline uint32_t RoundMinPowerOf2(uint32_t n, uint32_t min)
{
    assert((min & (min - 1)) == 0 && "Min alignment must be power of 2!");

    if (n <= min)
    {
        return min;
    }

    --n;

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    return n + 1;
}
} // namespace ECS
