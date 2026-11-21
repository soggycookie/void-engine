#pragma once
#include "logger/log.h"
#include "utils.h"

namespace VoidEngine
{

constexpr uint64_t DEFAULT_RESERVE_SIZE = mib(16);
constexpr uint64_t DEFAULT_COMMIT_SIZE = kib(16);

class Arena
{
private:
    uint64_t reserve_size_;
    uint64_t commit_size_;
    uint64_t pos_;
    void *addr_;

    void ensure_committed(uint64_t next_pos);

public:
    Arena() : reserve_size_(0), commit_size_(0), pos_(0), addr_(nullptr) {}

    NO_COPY_CTOR(Arena);
    NO_COPY_ASSIGNMENT(Arena);

    Arena(Arena &&other) : reserve_size_(other.reserve_size_), commit_size_(other.commit_size_), pos_(other.pos_), addr_(other.addr_)
    {
        other.addr_ = nullptr;
        other.commit_size_ = 0;
        other.reserve_size_ = 0;
        other.pos_ = 0;
    }

    void init(uint64_t reserve_size = DEFAULT_RESERVE_SIZE,
              uint64_t commit_size = DEFAULT_COMMIT_SIZE);
    void *push(uint64_t size, uint64_t align, uint64_t *out_pos = nullptr);
    void *in_place_grow(void *p, uint64_t old_size, uint64_t new_size, uint64_t align);
    void pop_to(uint64_t pos);
    void clear();
    void destroy();

    uint64_t get_pos() const { return pos_; }
    void *get_base_address() const { return addr_; }
    uint64_t get_commit_size() const { return commit_size_; }
};

struct ScopedArenaTemp
{
    Arena *arena;
    uint64_t pos;

    ScopedArenaTemp() : arena(nullptr), pos(0) {}

    NO_COPY_ASSIGNMENT(ScopedArenaTemp);
    NO_COPY_CTOR(ScopedArenaTemp);
    NO_MOVE_ASSIGNMENT(ScopedArenaTemp);
    NO_MOVE_CTOR(ScopedArenaTemp);

    void start_scratch(Arena *arena)
    {
        ENGINE_ASSERT(arena, "Arena is null!");
        this->arena = arena;
        pos = arena->get_pos();
    }

    void end_scratch()
    {
        ENGINE_ASSERT(arena, "Arena is null!");
        arena->pop_to(pos);
    }
};
using ScopedScratchTemp = ScopedArenaTemp;

class ThreadScratch
{
private:
    Arena scratches[2];

public:
    ThreadScratch() : scratches() {}

    Arena *get_scratch(Arena *arg_arena = nullptr)
    {
        return (arg_arena && arg_arena->get_base_address() == scratches[0].get_base_address())
                   ? &scratches[1]
                   : &scratches[0];
    }

    void init()
    {
        scratches[0].init(mib(1), kib(4));
        scratches[1].init(mib(1), kib(4));
    }

    void destroy()
    {
        scratches[0].destroy();
        scratches[1].destroy();
    }
};

extern thread_local ThreadScratch t_scratches;

template <typename T, typename... Args>
T *construct(Arena &arena, Args &&...args)
{
    void *addr = arena.push(sizeof(T), alignof(T));
    ENGINE_ASSERT(addr, "Failed to construct object!");
    return new (addr) T(std::forward<Args>(args)...);
}

template <typename T>
T *arena_push_array(Arena &arena, uint64_t count)
{
    void *addr = arena.push(sizeof(T) * count, alignof(T));
    ENGINE_ASSERT(addr, "Failed to allocate array!");
    return SCAST(T *, addr);
}
} // namespace VoidEngine
