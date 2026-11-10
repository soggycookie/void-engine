#pragma once
#include "utils.h"

namespace VoidEngine
{
class Vallocator;
class Arena;
class TLFS;

class String;

class StringView
{
private:
    const char8_t *data_;
    uint64_t length_;

public:
    StringView() : data_(nullptr), length_(0)
    {
    }

    StringView(const char8_t *addr) : data_(addr), length_(0)
    {
        length_ = addr ? str_len(addr) : 0;
    }

    StringView(const char8_t *addr, uint64_t length) : data_(addr), length_(length)
    {
    }

    StringView(const String &str);
    StringView(const String &str, uint64_t length);

    bool is_empty() const
    {
        return length_ == 0;
    }

    uint32_t get_length() const
    {
        return length_;
    }

    const char8_t *data() const
    {
        return data_;
    }

    friend std::ostream &operator<<(std::ostream &os, const StringView &view)
    {
        return os.write(RCAST(const char *, view.data_), view.length_);
    }

    bool operator==(StringView other)
    {
        return (length_ == other.length_) && (std::memcmp(data_, other.data_, length_) == 0);
    }

    bool operator!=(StringView other)
    {
        return !(*this == other);
    }
};

class String
{
private:
    char8_t *data_;
    uint64_t count_;
    uint64_t capacity_;

public:
    String() : data_(nullptr), count_(0), capacity_(0)
    {
    }

    String(String &&other) noexcept
        : data_(other.data_), count_(other.count_), capacity_(other.capacity_)
    {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.count_ = 0;
    }

    NO_COPY_ASSIGNMENT(String);
    NO_COPY_CTOR(String);
    NO_MOVE_ASSIGNMENT(String);

    void init(Arena &arena, uint64_t capacity);
    void init(Vallocator &vallocator, uint64_t capacity);
    void destroy(Vallocator &vallocator);

    void assign(Arena &arena, StringView view);
    void assign(Vallocator &vallocator, StringView view);
    void assign(Arena &arena, const String &str);
    void assign(Vallocator &vallocator, const String &str);

    void append(Arena &arena, StringView view);
    void append(Vallocator &vallocator, StringView view);
    // void move(String &&other);

    // template<typename T>
    // void assign(Arena &arena, T &&str)
    // {
    //     using C = std::remove_cvref_t<T>;
    //     static_assert(std::is_same_v<C, String>, "Type is not suitable for string assignment");
    //     move(arena, std::forward<T>(str));
    // }
    //
    // template<typename T>
    // void assign(Vallocator &vallocator, T &&str)
    // {
    //     using C = std::remove_cvref_t<T>;
    //     static_assert(std::is_same_v<C, String>, "Type is not suitable for string assignment");
    //     move(vallocator, std::forward<T>(str));
    // }
    void clear();

    bool is_empty() const
    {
        return count_ == 0;
    }

    uint32_t get_length() const
    {
        return count_;
    }

    char8_t *mut_data()
    {
        return data_;
    }

    char8_t *data() const
    {
        return data_;
    }

    friend std::ostream &operator<<(std::ostream &os, const String &str)
    {
        return os.write(RCAST(const char *, str.data_), str.count_);
    }

private:
    void resize(Arena &arena, uint32_t new_capacity);
    void resize(Vallocator &vallocator, uint32_t new_capacity);
};

} // namespace VoidEngine
