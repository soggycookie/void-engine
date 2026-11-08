#pragma once
#include "arena.h"
#include "logger/log.h"
#include "utils.h"
#include <source_location>

namespace VoidEngine
{
class TLFS
{
private:
#ifdef VOID_DEBUG
    struct DebugTag
    {
        DebugTag() : file(nullptr), line(0), magic(DEAD_MAGIC_NUMBER)
        {
        }

        const char *file;
        int32_t line;
        int32_t magic;
    };

    static constexpr uint32_t MAGIC_NUMBER = 0xA7F3C91D;
    static constexpr uint32_t DEAD_MAGIC_NUMBER = 0xDEADDEAD;
#endif // VOID_DEBUG

    struct Header
    {
        uint64_t size;
#ifdef VOID_DEBUG
        DebugTag debug_tag;

        Header() : size(0), debug_tag()
        {
        }
#else
        Header() : size(0)
        {
        }
#endif // VOID_DEBUG
    };

    struct Block
    {
        Header header;
        Block *next_free;
        Block *prev_free;
    };

    struct Index
    {
        uint16_t fli;
        uint16_t sli;

        DEFAULT_COMPARABLE(Index);
    };

private:
#ifdef VOID_DEBUG
    static constexpr uint32_t MIN_BLOCK_SIZE = 64;
#else
    static constexpr uint32_t MIN_BLOCK_SIZE = 32;
#endif // VOID_DEBUG
    static constexpr uint32_t MIN_PAYLOAD_SIZE = MIN_BLOCK_SIZE - sizeof(Header);
    static constexpr uint32_t SMALL_BUCKET_SIZE = 256; // bucket 1
    static constexpr uint32_t SL_BITSHIFT = 5;
    static constexpr uint32_t SL_COUNT = 1U << SL_BITSHIFT;
    static constexpr uint32_t FL_COUNT = 24;
    static constexpr uint32_t FREELIST_COUNT = FL_COUNT * SL_COUNT;
    static constexpr Index INVALID_INDEX = {FL_COUNT, SL_COUNT};

    static const Header POOL_SENTINEL;

    void *addr;
    uint32_t sl_bitmaps[FL_COUNT];
    Block *freelists[FREELIST_COUNT];
    uint32_t fl_bitmap;
    uint32_t alloc_count;
    uint64_t alloc_size;
    uint64_t commit_size;
    uint64_t reserve_size;
#ifdef VOID_DEBUG
    char name[64];
#endif
public:
    TLFS() : fl_bitmap(0), alloc_count(0), alloc_size(0), freelists(nullptr), commit_size(0), reserve_size(0), addr(nullptr)
    {
    }

    NO_COPY_CTOR(TLFS);
    NO_COPY_ASSIGNMENT(TLFS);

    TLFS(TLFS &&other) noexcept
    {
        std::memcpy(sl_bitmaps, other.sl_bitmaps, sizeof(sl_bitmaps));
        std::memcpy(freelists, other.freelists, sizeof(freelists));

        fl_bitmap = other.fl_bitmap;
        alloc_count = other.alloc_count;
        alloc_size = other.alloc_size;
        commit_size = other.commit_size;
        reserve_size = other.reserve_size;
        std::memcpy(name, other.name, 64);

        std::memset(other.sl_bitmaps, 0, sizeof(sl_bitmaps));
        std::memset(other.freelists, 0, sizeof(freelists));

        alloc_count = 0;
        alloc_size = 0;
        addr = other.addr;
        other.addr = nullptr;
    }

    // Vallocator &operator=(Vallocator &&other)
    // {
    //     Destroy();
    //
    //     std::memcpy(sl_bitmaps, other.sl_bitmaps, sizeof(sl_bitmaps));
    //     std::memcpy(freelists, other.freelists, sizeof(freelists));
    //
    //     fl_bitmap = other.fl_bitmap;
    //     alloc_count = other.alloc_count;
    //     total_size = other.total_size;
    //     alloc_size = other.alloc_size;
    //     pool_size = other.pool_size;
    //     pool_head = other.pool_head;
    //
    //     std::memset(other.sl_bitmaps, 0, sizeof(sl_bitmaps));
    //     std::memset(other.freelists, 0, sizeof(freelists));
    //
    //     alloc_count = 0;
    //     total_size = 0;
    //     alloc_size = 0;
    //     pool_head = nullptr;
    //
    //     return *this;
    // }

#ifdef VOID_DEBUG
    void *tlfs_alloc(uint64_t size, uint64_t align, std::source_location loc = std::source_location::current());
    void tlfs_free(void *addr, std::source_location loc = std::source_location::current());
    void *tlfs_realloc(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align,
                       std::source_location loc = std::source_location::current());
    void set_name(const char *name);
    void print();
#else
    void *tlfs_alloc(uint64_t size, uint64_t align);
    void *tlfs_realloc(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align);
    void tlfs_free(void *addr);
#endif // VOID_DEBUG

    uint32_t get_alloc_count() const
    {
        return alloc_count;
    }
    uint64_t get_alloc_size() const
    {
        return alloc_size;
    }
    uint64_t get_min_alloc_size() const
    {
        return MIN_PAYLOAD_SIZE;
    }

    void init(uint64_t res_size, uint64_t com_size);
    void destroy();

private:
    void *tlfs_alloc_internal(uint64_t size, uint64_t align);
    void *tlfs_realloc_internal(void *old_ptr, uint64_t old_size, uint64_t new_size, uint64_t align);
    void tlfs_free_internal(void *addr);

    uint64_t acquire_memory(uint64_t alloc_size);

    Index round_up_to_bin(uint64_t size);
    Index round_down_to_bin(uint64_t size);
    Index find_next_free_index(Index start_index);

    void *get_payload(Block *block);
    Header *get_block_header(Block *block);
    uint64_t calc_payload_size(uint64_t size);
    uint64_t get_payload_size(Block *block);
    uint64_t get_payload_size(Header *header);

    void insert_free_block(Block *block);
    Block *acquire_block_from_freelist(Index index);
    void remove_free_block(Block *block);

    Block *split(Block *block, uint64_t size);
    bool can_split(Block *block, uint64_t size);
    Block *coalesce(Block *prevBlock, Block *block);

    void mark_used(Block *block);
    void mark_free(Block *block);

    Block *get_block_from_payload(void *addr);

    bool is_used_block(Header *size);
    bool is_prev_physical_free(Block *block);
    bool is_next_physical_free(Block *block);
    bool is_address_valid(void *p);
    bool is_header_deadend(Header *header) const;
};

class Pool
{
private:
    struct Node
    {
        Node *next;
    };

private:
    Arena *page_arena;
    Node *head;
    uint64_t alloc_count;
    uint64_t page_count;
    uint64_t size_class;

public:
    Pool() : page_arena(nullptr), head(nullptr), alloc_count(0), page_count(0), size_class(0)
    {
    }

    void init(Arena *arena, uint64_t size_class);
    void *pool_alloc();
    void pool_free(void *addr);

private:
    void acquire_page();
};

template <typename Key, typename Value>
class FlatHashMap;

class Vallocator
{
private:
    constexpr static uint64_t MIN_SIZE_CLASS = 16;
    constexpr static uint64_t MAX_SIZE_CLASS = 4096;
    constexpr static uint64_t POOL_COUNT = floor_log2(MAX_SIZE_CLASS) - floor_log2(MIN_SIZE_CLASS) + 1;
    TLFS tlfs;
    Arena page_arena;
    Pool pools[POOL_COUNT];
    uint64_t alloc_count;
#ifdef VOID_DEBUG
    FlatHashMap<uint64_t, std::source_location> *debug_map;
#endif // VOID_DEBUG

public:
    Vallocator() : tlfs(), page_arena(), alloc_count(0)
#ifdef VOID_DEBUG
        , debug_map(nullptr)
#endif // VOID_DEBUG
    {
    }

    void init(uint64_t tlfs_reserve_size, uint64_t tlfs_commit_size, uint64_t arena_reserve_size,
              uint64_t arena_commit_size);
#ifdef VOID_DEBUG
    void *valloc(uint64_t size, uint64_t align, std::source_location loc = std::source_location::current());
    void vfree(uint64_t size, void *addr, std::source_location loc = std::source_location::current());
#else
    void *valloc(uint64_t size, uint64_t align);
    void vfree(uint64_t size, void *addr);
#endif // VOID_DEBUG
    void destroy();

    TLFS &get_tlfs()
    {
        return tlfs;
    }
    Arena &get_arena()
    {
        return page_arena;
    }
};

template <typename T, typename... Args>
T *construct(TLFS &allocator, Args &&...args)
{
    void *addr = allocator.tlfs_alloc(sizeof(T), alignof(T));
    ENGINE_ASSERT(addr, "Failed to construct object!");
    return new (addr) T(std::forward<Args>(args)...);
}

template <typename T>
void destruct(TLFS &allocator, T *data)
{
    if (!data)
    {
        return;
    }
    data->~T();
    allocator.tlfs_free(data);
}

template <typename T, typename... Args>
T *construct(Vallocator &allocator, Args &&...args)
{
    void *addr = allocator.valloc(sizeof(T), alignof(T));
    ENGINE_ASSERT(addr, "Failed to construct object!");
    return new (addr) T(std::forward<Args>(args)...);
}

template <typename T>
void destruct(Vallocator &allocator, T *data)
{
    if (!data)
    {
        return;
    }
    data->~T();
    allocator.vfree(sizeof(T), data);
}

template <typename T>
T *tlfs_alloc_array(TLFS &allocator, uint64_t count, std::source_location loc = std::source_location::current())
{
    void *addr = allocator.tlfs_alloc(sizeof(T) * count, alignof(T), loc);
    ENGINE_ASSERT(addr, "Failed to allocate array!");
    return SCAST(T *, addr);
}

template <typename T>
void tlfs_free_array(TLFS &allocator, T *addr, std::source_location loc = std::source_location::current())
{
    allocator.tlfs_free(addr, loc);
}

template <typename T>
T *valloc_array(Vallocator &allocator, uint64_t count, std::source_location loc = std::source_location::current())
{
    void *addr = allocator.valloc(sizeof(T) * count, alignof(T), loc);
    ENGINE_ASSERT(addr, "Failed to allocate array!");
    return SCAST(T *, addr);
}

template <typename T>
void valloc_free_array(Vallocator &allocator, T *addr, uint64_t count, std::source_location loc = std::source_location::current())
{
    allocator.vfree(sizeof(T) * count, addr, loc);
}
} // namespace VoidEngine
