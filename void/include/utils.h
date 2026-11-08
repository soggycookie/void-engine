#pragma once
#include "pch.h"
namespace VoidEngine
{

#define SCAST(T, v) (static_cast<T>(v))
#define RCAST(T, v) (reinterpret_cast<T>(v))
#define DCAST(T, v) (dynamic_cast<T>(v))

#define OFFSET_ADDR(addr, offset)      RCAST(void *, RCAST(uintptr_t, addr) + (offset))
#define OFFSET_T_ADDR(T, addr, offset) RCAST(T, RCAST(uintptr_t, addr) + (offset))
#define BYTE_OFFSET(v1, v2)            SCAST(size_t, RCAST(uintptr_t, v1) - RCAST(uintptr_t, v2))

#define DEFINE_HANDLE(name)                                                                        \
    struct name##_T;                                                                               \
    using name = name##_T *;

constexpr uint64_t DEFAULT_ALIGNMENT = 8;

inline uint32_t str_len(const char8_t *s)
{
    return std::strlen(RCAST(const char *, s));
}

constexpr uint32_t floor_log2(uint32_t n)
{
    return std::bit_width(n) - 1;
}

constexpr uint64_t floor_log2(uint64_t n)
{
    return std::bit_width(n) - 1;
}
constexpr bool is_power_of_2(size_t size) { return (size & (size - 1)) == 0; }

constexpr uint32_t next_power_of_2(uint32_t v)
{
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    return v;
}

constexpr uint64_t next_power_of_2(uint64_t v)
{
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    ++v;
    return v;
}

constexpr uint64_t align_up(uint64_t size, uint64_t align)
{
    return (size + align - 1) & ~(align - 1);
}

constexpr uint64_t align_down(uint64_t size, uint64_t alignment) { return size & ~(alignment - 1); }

inline void *align_up(void *addr, uint64_t align)
{
    uintptr_t alignedAddr = (RCAST(uintptr_t, addr) + align - 1) & ~(align - 1);
    return RCAST(void *, alignedAddr);
}

inline void *align_down(void *addr, uint64_t align)
{
    uintptr_t alignedAddr = RCAST(uintptr_t, addr) & ~(align - 1);
    return RCAST(void *, alignedAddr);
}

inline bool is_address_in_range(void *addr, void *start, uint64_t size)
{
    // ENGINE_ASSERT(addr, "Address must not be null!");
    uintptr_t startAddr = RCAST(uintptr_t, start);
    uintptr_t endAddr = startAddr + size;
    uintptr_t address = RCAST(uintptr_t, addr);

    return address >= startAddr && address < endAddr;
}

constexpr uint64_t kib(uint64_t kib) { return kib * 1024; }
constexpr uint64_t mib(uint64_t mib) { return mib * 1024 * 1024; }
constexpr uint64_t gib(uint64_t gib) { return gib * 1024 * 1024 * 1024; }

constexpr double to_kib(uint64_t b) { return SCAST(double, b) / (1024.0); }
constexpr double to_mib(uint64_t b) { return SCAST(double, b) / (1024.0 * 1024.0); }
constexpr double to_gib(uint64_t b) { return SCAST(double, b) / (1024.0 * 1024.0 * 1024.0); }

constexpr uint32_t low_32bit(uint64_t n)  { return SCAST(uint32_t, n); }
constexpr uint32_t high_32bit(uint64_t n) { return SCAST(uint32_t, n >> 32U); }
constexpr uint64_t make_uint64(uint32_t low, uint32_t high)
{
    uint64_t r = SCAST(uint64_t, low) | (SCAST(uint64_t, high) << 32);
    return r;
}

constexpr uint64_t bump_high_32bit(uint64_t n)
{
    uint32_t l = low_32bit(n);
    uint32_t h = high_32bit(n);
    ++h;
    return make_uint64(l, h);
}

inline void *aligned_alloc(size_t alignment, size_t size)
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    size_t alignedSize = AlignUp(size, alignment);
    return std::aligned_alloc(alignment, alignedSize);
#endif
}

inline void aligned_free(void *ptr)
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

#define NO_COPY_CTOR(T)       T(const T &) = delete
#define NO_MOVE_CTOR(T)       T(T &&) = delete
#define NO_COPY_ASSIGNMENT(T) T &operator=(const T &) = delete
#define NO_MOVE_ASSIGNMENT(T) T &operator=(T &&) = delete

#define DEFAULT_CTOR(T) T() = default
#define EXPLICIT_DTOR(T) ~T() {}
#define DEFAULT_VIRTUAL_DTOR(T)    virtual ~T() = default
#define DEFAULT_COPY_CTOR(T)       T(const T &) = default
#define DEFAULT_MOVE_CTOR(T)       T(T &&) = default
#define DEFAULT_COPY_ASSIGNMENT(T) T &operator=(const T &) = default
#define DEFAULT_MOVE_ASSIGNMENT(T) T &operator=(T &&) = default

#define DEFAULT_COMPARABLE(T) auto operator<=>(const T &) const = default

inline constexpr uint32_t k_baseContainerCapacity = 8;
inline constexpr size_t k_pageSize = kib(4);

using TimePoint = std::chrono::system_clock::time_point;


} // namespace VoidEngine
