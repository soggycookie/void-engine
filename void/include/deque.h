#pragma once
#include "arena.h"
#include "utils.h"
#include "vallocator.h"

namespace VoidEngine
{

template <typename T>
class Deque
{
private:
    T *data_;
    uint64_t head_;
    uint64_t tail_;
    uint64_t capacity_;

public:
    Deque() : head_(0), tail_(0), capacity_(0), data_(nullptr)
    {
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible!");
        static_assert(std::is_move_constructible<T>() || std::is_copy_constructible<T>(),
                      "T must be move or copy constructible!");
        static_assert(std::is_destructible_v<T>, "T must be destructible!");
    }

    void init(Arena &arena, uint64_t capacity)
    {
        ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
        capacity_ = next_power_of_2(capacity);
        data_ = arena_push_array<T>(arena, capacity_);
        ENGINE_ASSERT(data_, "Failed to allocate!");
    }

    template <typename U>
    void push_back(Arena &arena, U &&data)
    {
        // avoid tail_ wrap back to head_
        if (get_count() == capacity_ - 1)
        {
            resize(arena, capacity_ * 2);
        }

        new (data_ + tail_) T(std::forward<U>(data));
        tail_ = (tail_ + 1) & (capacity_ - 1);
    }

    template <typename U>
    void push_front(Arena &arena, U &&data)
    {
        // avoid tail_ wrap back to head_
        if (get_count() == capacity_ - 1)
        {
            resize(arena, capacity_ * 2);
        }

        head_ = (head_ - 1) & (capacity_ - 1);
        new (data_ + head_) T(std::forward<U>(data));
    }

    void pop_front()
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            data_[head_].~T();
        }
        head_ = (head_ + 1) & (capacity_ - 1);
    }

    void pop_back()
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");
        tail_ = (tail_ - 1) & (capacity_ - 1);
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            data_[tail_].~T();
        }
    }

    void resize(Arena &arena, uint64_t new_capacity)
    {
        ENGINE_ASSERT(new_capacity > capacity_, "New capacity must be greater than old capacity!");
        ENGINE_ASSERT(is_power_of_2(new_capacity), "New capacity must be power of 2!");
        T *new_data = arena_push_array<T>(arena, new_capacity);
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            if (head_ <= tail_)
            {
                std::memcpy(new_data, data_ + head_, (tail_ - head_) * sizeof(T));
            }
            else
            {
                std::memcpy(new_data, data_ + head_, (capacity_ - head_) * sizeof(T));
                if (tail_ != 0)
                {
                    std::memcpy(new_data + (capacity_ - head_), data_, tail_ * sizeof(T));
                }
            }
        }
        else
        {
            uint64_t idx = 0;
            while (!is_empty())
            {
                T &d = front();
                new (new_data + idx) T(std::move(d));
                pop_front();
                ++idx;
            }
        }
        head_ = 0;
        tail_ = capacity_ - 1;
        capacity_ = new_capacity;
        data_ = new_data;
    }

    void clear()
    {
        if constexpr (!std::is_trivially_copyable_v<T>)
        {
            while (!is_empty())
            {
                T &d = front();
                pop_front();
            }
        }
        head_ = 0;
        tail_ = 0;
    }

    void destroy(Vallocator &vallocator)
    {
        clear();
        vallocator.vfree(sizeof(T) * capacity_, data_);
    }

    uint64_t get_count() const
    {
        return (tail_ - head_) & (capacity_ - 1);
    }

    bool is_empty() const
    {
        return tail_ == head_;
    }

    T &front()
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");
        return data_[head_];
    }

    const T &front() const
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");
        return data_[head_];
    }

    const T &back() const
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");
        return data_[(tail_ - 1) & (capacity_ - 1)];
    }

    T &back()
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");
        return data_[(tail_ - 1) & (capacity_ - 1)];
    }
};

} // namespace VoidEngine
