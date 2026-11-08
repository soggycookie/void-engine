#pragma once
#include "arena.h"
#include "logger/log.h"
#include "utils.h"
#include "vallocator.h"

namespace VoidEngine
{
template <typename T>
class DynamicArray
{
private:
    T *data_;
    uint64_t capacity_;
    uint64_t count_;

public:
    struct Iterator
    {
    private:
        friend class DynamicArray;
        DynamicArray<T> *m_array;
        T *m_ptr;

    private:
        Iterator(T *ptr, DynamicArray<T> *array) : m_ptr(ptr), m_array(array)
        {
        }

        bool is_valid() const
        {
            return m_ptr && m_array &&
                   is_address_in_range(m_ptr, m_array->data_, m_array->capacity_ * sizeof(T));
        }

    public:
        T &operator*()
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            return *m_ptr;
        }

        const T &operator*() const
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            return *m_ptr;
        }

        uint64_t index() const
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            uintptr_t end = RCAST(uintptr_t, m_array->data_ + m_array->capacity_);
            uintptr_t start = RCAST(uintptr_t, m_array->data_);
            uintptr_t cur = RCAST(uintptr_t, m_ptr);
            ENGINE_ASSERT(cur >= start, "Invalid address!");
            ENGINE_ASSERT(end > start && end > cur, "Invalid address!");
            uint64_t index = (cur - start) / sizeof(T);
            ENGINE_ASSERT(index <= m_array->count_ - 1, "Iterator is out of bound!");

            return index;
        }

        T *operator-()
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            return m_ptr;
        }

        const T *operator-() const
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            return m_ptr;
        }

        Iterator &operator++()
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            ++m_ptr;
            return *this;
        }

        Iterator operator++(int)
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator &operator--()
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            --m_ptr;
            return *this;
        }

        Iterator operator--(int)
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        Iterator operator-(int64_t index) const
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            return Iterator(m_ptr - index, m_array);
        }

        Iterator operator+(int64_t index) const
        {
            ENGINE_ASSERT(is_valid(), "Invalid Iterator!");
            return Iterator(m_ptr + index, m_array);
        }

        bool operator==(const Iterator &other) const
        {
            return m_ptr == other.m_ptr && m_array == other.m_array;
        }

        bool operator!=(const Iterator &other) const
        {
            return m_ptr != other.m_ptr && m_array == other.m_array;
        }
    };

public:
    explicit DynamicArray() : count_(0), capacity_(0), data_(nullptr)
    {
        static_assert(std::is_destructible_v<T>, "Type must be destructible!");
        static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                      "Type must be copy or move constructible!");
    }

    NO_MOVE_ASSIGNMENT(DynamicArray);
    NO_COPY_CTOR(DynamicArray);
    NO_COPY_ASSIGNMENT(DynamicArray);

    DynamicArray(DynamicArray &&other) noexcept
        : data_(other.data_), capacity_(other.capacity_), count_(other.count_)
    {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.count_ = 0;
    }

    void init(Arena &arena, uint64_t capacity)
    {
        ENGINE_ASSERT(!data_, "Array is already init");
        ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
        data_ = SCAST(T *, arena.push(capacity * sizeof(T), alignof(T), nullptr));
        ENGINE_ASSERT(data_, "Failed to alloc array!");
        capacity_ = capacity;
    }

    void init(Vallocator &vallocator, uint64_t capacity,
              std::source_location loc = std::source_location::current())
    {
        ENGINE_ASSERT(!data_, "Array is already init");
        ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
        data_ = SCAST(T *, vallocator.valloc(capacity * sizeof(T), alignof(T), loc));
        ENGINE_ASSERT(data_, "Failed to alloc array!");
        capacity_ = capacity;
    }

    void destroy(Vallocator &vallocator)
    {
        clear();
        vallocator.vfree(sizeof(T) * capacity_, data_);
        data_ = nullptr;
        capacity_ = 0;
    }

    template <typename U>
    void push_back(Arena &arena, U &&value)
    {
        static_assert(std::is_constructible_v<T, U &&>, "T must be constructible from U&&");
        if (count_ == capacity_)
        {
            resize(arena, std::ceil(capacity_ * 1.5f));
        }
        void *p = (data_ + count_);
        new (p) T(std::forward<U>(value));
        count_++;
    }

    template <typename U>
    void push_back(Vallocator &vallocator, U &&value)
    {
        static_assert(std::is_constructible_v<T, U &&>, "T must be constructible from U&&");
        if (count_ == capacity_)
        {
            resize(vallocator, std::ceil(capacity_ * 1.5f));
        }
        void *p = (data_ + count_);
        new (p) T(std::forward<U>(value));
        count_++;
    }

    template <typename... Args>
    void emplace_back(Arena &arena, Args &&...args)
    {
        if (count_ >= capacity_)
        {
            resize(arena, std::ceil(capacity_ * 1.5f));
        }
        void *p = (data_ + count_);
        new (p) T(std::forward<Args>(args)...);
        count_++;
    }

    template <typename... Args>
    void emplace_back(Vallocator &vallocator, Args &&...args)
    {
        if (count_ >= capacity_)
        {
            resize(vallocator, std::ceil(capacity_ * 1.5f));
        }
        void *p = (data_ + count_);
        new (p) T(std::forward<Args>(args)...);
        count_++;
    }

    Iterator find(const T &value)
    {
        for (auto it = begin(); it != end(); it++)
        {
            if (*it == value)
            {
                return it;
            }
        }

        return end();
    }

    uint64_t get_count() const
    {
        return count_;
    }

    uint64_t get_capacity() const
    {
        return capacity_;
    }

    void pop_back()
    {
        ENGINE_ASSERT(!is_empty(), "Array is empty!");
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            data_[count_ - 1].~T();
        }
        --count_;
    }

    bool is_empty() const
    {
        return count_ == 0;
    }

    void clear()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (uint64_t idx = 0; idx < count_; ++idx)
            {
                data_[idx].~T();
            }
        }
        count_ = 0;
    }

    T &operator[](uint64_t index)
    {
        if (index >= count_)
        {
            ENGINE_ASSERT(false, "Access out of bound index!");
        }
        T *element = data_ + index;
        return *element;
    }

    const T &operator[](uint64_t index) const
    {
        if (index >= count_)
        {
            ENGINE_ASSERT(false, "Access out of bound index!");
        }
        T *element = data_ + index;
        return *element;
    }

    Iterator begin()
    {
        ENGINE_ASSERT(data_, "Array does not allocate yet!");
        return Iterator(data_, this);
    }

    Iterator end()
    {
        ENGINE_ASSERT(data_, "Array does not allocate yet!");
        return Iterator(data_ + count_, this);
    }

private:
    void resize(Arena &arena, uint64_t new_capacity)
    {
        T *new_data = SCAST(T *, arena.in_place_grow(data_, sizeof(T) * capacity_,
                                                     sizeof(T) * new_capacity, alignof(T)));
        if (new_data != data_)
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                std::memcpy(new_data, data_, count_ * sizeof(T));
            }
            else
            {
                for (uint64_t idx = 0; idx < count_; idx++)
                {
                    T *dst = new_data + idx;
                    T *src = data_ + idx;

                    if constexpr (std::is_move_constructible_v<T>)
                    {
                        new (dst) T(std::move(*src));
                    }
                    else
                    {
                        new (dst) T(*src);
                    }

                    src->~T();
                }
            }
            data_ = new_data;
        }
        capacity_ = new_capacity;
    }

    void resize(Vallocator &vallocator, uint64_t new_capacity)
    {
        T *new_addr = SCAST(T *, vallocator.valloc(new_capacity * sizeof(T), alignof(T)));
        ENGINE_ASSERT(new_addr, "Failed to resize array!");
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            std::memcpy(new_addr, data_, count_ * sizeof(T));
        }
        else
        {
            for (uint64_t idx = 0; idx < count_; idx++)
            {
                T *dst = new_addr + idx;
                T *src = data_ + idx;

                if constexpr (std::is_move_constructible_v<T>)
                {
                    new (dst) T(std::move(*src));
                }
                else
                {
                    new (dst) T(*src);
                }

                src->~T();
            }
        }
        vallocator.vfree(capacity_ * sizeof(T), data_);
        data_ = new_addr;
        capacity_ = new_capacity;
    }

    bool is_iter_valid(Iterator it)
    {
        return is_address_in_range(it.m_ptr, data_, sizeof(T) * capacity_);
    }

    friend void swap(DynamicArray &lhs, DynamicArray &rhs) noexcept
    {
        std::swap(lhs.data_, rhs.data_);
        std::swap(lhs.count_, rhs.count_);
        std::swap(lhs.capacity_, rhs.capacity_);
    }
};

} // namespace VoidEngine
