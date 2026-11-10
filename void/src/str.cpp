#include "str.h"
#include "arena.h"
#include "logger/log.h"
#include "vallocator.h"

namespace VoidEngine
{

StringView::StringView(const String &str) : data_(str.data()), length_(str.get_length())
{
}

StringView::StringView(const String &str, uint64_t length) : data_(str.data()), length_(length)
{
}

void String::init(Arena &arena, uint64_t capacity)
{
    ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
    capacity_ = next_power_of_2(capacity);
    data_ = SCAST(char8_t *, arena.push(capacity_, alignof(char8_t), nullptr));
    ENGINE_ASSERT(data_, "Failed to init string!");
}

void String::init(Vallocator &vallocator, uint64_t capacity)
{
    ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
    capacity_ = next_power_of_2(capacity);
    data_ = SCAST(char8_t *, vallocator.valloc(capacity_, alignof(char8_t)));
    ENGINE_ASSERT(data_, "Failed to init string!");
}

void String::destroy(Vallocator &vallocator)
{
    vallocator.vfree(capacity_ * sizeof(char8_t), data_);
}

void String::assign(Arena &arena, const String &str)
{
    if (str.count_ > capacity_)
    {
        resize(arena, str.count_);
    }
    std::memcpy(data_, str.data_, str.count_ * sizeof(char8_t));
    count_ = str.get_length();
}

void String::assign(Vallocator &vallocator, const String &str)
{
    if (str.count_ > capacity_)
    {
        resize(vallocator, str.count_);
    }
    std::memcpy(data_, str.data_, str.count_ * sizeof(char8_t));
    count_ = str.get_length();
}

// void String::move(String &&other)
// {
//     addr = other.addr;
//     count = other.count;
//     capacity = other.capacity;
//     other.addr = nullptr;
//     other.count = 0;
//     other.capacity = 0;
// }

void String::assign(Arena &arena, StringView view)
{
    if (view.get_length() > capacity_)
    {
        resize(arena, view.get_length());
    }
    std::memcpy(data_, view.data(), view.get_length() * sizeof(char8_t));
    count_ = view.get_length();
}

void String::assign(Vallocator &vallocator, StringView view)
{
    if (view.get_length() > capacity_)
    {
        resize(vallocator, view.get_length());
    }
    std::memcpy(data_, view.data(), view.get_length() * sizeof(char8_t));
    count_ = view.get_length();
}

void String::append(Arena &arena, StringView view)
{
    if (view.get_length() + count_ > capacity_)
    {
        resize(arena, view.get_length() + count_);
    }
    std::memcpy(data_ + count_, view.data(), view.get_length() * sizeof(char8_t));
    count_ += view.get_length();
}

void String::append(Vallocator &vallocator, StringView view)
{
    if (view.get_length() + count_ > capacity_)
    {
        resize(vallocator, view.get_length() + count_);
    }
    std::memcpy(data_ + count_, view.data(), view.get_length() * sizeof(char8_t));
    count_ += view.get_length();
}

void String::resize(Arena &arena, uint32_t new_capacity)
{
    // if(data_)
    // {
    //     init(arena, new_capacity);
    //     return;
    // }
    new_capacity = next_power_of_2(new_capacity);
    char8_t *new_data = SCAST(char8_t *, arena.in_place_grow(data_, sizeof(char8_t) * capacity_,
                                                             sizeof(char8_t) * new_capacity, 1));
    ENGINE_ASSERT(new_data, "Failed to resize string!");
    std::memcpy(new_data, data_, count_ * sizeof(char8_t));
    data_ = new_data;
    capacity_ = new_capacity;
}

void String::resize(Vallocator &vallocator, uint32_t new_capacity)
{
    // if(data_)
    // {
    //     init(vallocator, new_capacity);
    //     return;
    // }
    new_capacity = next_power_of_2(new_capacity);
    char8_t *new_addr =
        SCAST(char8_t *, vallocator.valloc(new_capacity * sizeof(char8_t), alignof(char8_t)));
    ENGINE_ASSERT(new_addr, "Failed to resize string!");
    std::memcpy(new_addr, data_, count_ * sizeof(char8_t));
    vallocator.vfree(capacity_ * sizeof(char8_t), data_);
    data_ = new_addr;
    capacity_ = new_capacity;
}

void String::clear()
{
    std::memset(data_, 0, capacity_);
    count_ = 0;
}
} // namespace VoidEngine
