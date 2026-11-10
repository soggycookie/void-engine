#pragma once
#include "utils.h"
#include "vallocator.h"

/*
    Robin hood open addressing hash map
*/

namespace VoidEngine
{

template <typename T>
struct Hash;

// chatgpt
template <>
struct Hash<uint64_t>
{
    static uint64_t Value(uint64_t x)
    {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;

        return x;
    }
};

template <>
struct Hash<uint32_t>
{
    static uint64_t Value(uint32_t x)
    {
        return Hash<uint64_t>::Value(SCAST(uint64_t, x));
    }
};

template <typename Key, typename Value>
class FlatHashMap
{
private:
    struct Bucket
    {
        alignas(Key) char key[sizeof(Key)];
        alignas(Value) char value[sizeof(Value)];
        uint32_t PSL;
        uint8_t occupied;
        unsigned char padding[3];
    };

public:
    struct Iterator
    {
    private:
        friend class FlatHashMap;
        Bucket *m_ptr;

    private:
        Iterator(Bucket *ptr) : m_ptr(ptr)
        {
        }

    public:
        bool is_occupied() const
        {
            return m_ptr->occupied;
        }

        Value &get_val()
        {
            return *RCAST(Value *, m_ptr->value);
        }

        Key &get_key()
        {
            return *RCAST(Key *, m_ptr->key);
        }

        const Value &get_val() const
        {
            return *RCAST(Value *, m_ptr->value);
        }

        const Key &get_key() const
        {
            return *RCAST(Key *, m_ptr->key);
        }

        Iterator &operator++()
        {
            ++m_ptr;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator &operator--()
        {
            --m_ptr;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        Iterator operator-(int64_t index) const
        {
            return Iterator(m_ptr - index);
        }

        Iterator operator+(int64_t index) const
        {
            return Iterator(m_ptr + index);
        }

        bool operator==(const Iterator &other) const
        {
            return m_ptr == other.m_ptr;
        }

        bool operator!=(const Iterator &other) const
        {
            return m_ptr != other.m_ptr;
        }
    };

private:
    static constexpr float LOAD_FACTOR_THRESHOLD = 0.8f;

    Bucket *addr;
    uint64_t bucket_capacity;
    uint64_t count;

public:
    FlatHashMap() : addr(nullptr), bucket_capacity(0), count(0)
    {
        static_assert(std::is_move_constructible_v<Key> || std::is_copy_constructible_v<Key>);
        static_assert(std::is_move_constructible_v<Value> || std::is_copy_constructible_v<Value>);
        // static_assert(std::is_move_assignable_v<Value> || std::is_copy_assignable_v<Value>);
        // static_assert(std::is_move_assignable_v<Key> || std::is_copy_assignable_v<Key>);
        static_assert(std::is_destructible_v<Key> && std::is_destructible_v<Value>);
    }

    NO_MOVE_ASSIGNMENT(FlatHashMap);
    NO_COPY_ASSIGNMENT(FlatHashMap);
    NO_COPY_CTOR(FlatHashMap);

    FlatHashMap(FlatHashMap &&other) noexcept
        : addr(other.addr), bucket_capacity(other.bucket_capacity), count(other.count)
    {
        other.addr = nullptr;
        other.count = 0;
        other.bucket_capacity = 0;
    }

    void init(TLFS &allocator, uint64_t capacity)
    {
        ENGINE_ASSERT(!addr, "Data is already set!");
        ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
        bucket_capacity = next_power_of_2(capacity);
        addr = SCAST(Bucket *, allocator.tlfs_alloc(bucket_capacity * sizeof(Bucket), alignof(Bucket)));
        ENGINE_ASSERT(addr, "Failed to allocate hashmap!");
    }

    void init(Vallocator &vallocator, uint64_t capacity)
    {
        ENGINE_ASSERT(!addr, "Data is already set!");
        ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
        bucket_capacity = next_power_of_2(capacity);
        addr = SCAST(Bucket *, vallocator.valloc(bucket_capacity * sizeof(Bucket), alignof(Bucket)));
        ENGINE_ASSERT(addr, "Failed to allocate hashmap!");
    }

    void destroy(TLFS &allocator)
    {
        clear();
        allocator.tlfs_free(addr);
    }

    void destroy(Vallocator &vallocator)
    {
        clear();
        vallocator.vfree(bucket_capacity * sizeof(Bucket), addr);
    }

    template <typename K, typename V>
    void insert(TLFS &allocator, K &&key, V &&value)
    {
        static_assert(std::is_constructible_v<Key, K &&>, "Key must be constructible from K&&");
        static_assert(std::is_constructible_v<Value, V &&>, "Value must be constructible from V&&");

        if (SCAST(float, count) / SCAST(float, bucket_capacity) >= LOAD_FACTOR_THRESHOLD)
        {
            resize(allocator, bucket_capacity * 2);
        }

        if (insert_internal(std::forward<K>(key), std::forward<V>(value), addr, bucket_capacity))
        {
            ++count;
        }
    }

    template <typename K, typename V>
    void insert(Vallocator &vallocator, K &&key, V &&value)
    {
        static_assert(std::is_constructible_v<Key, K &&>, "Key must be constructible from K&&");
        static_assert(std::is_constructible_v<Value, V &&>, "Value must be constructible from V&&");

        if (SCAST(float, count) / SCAST(float, bucket_capacity) >= LOAD_FACTOR_THRESHOLD)
        {
            resize(vallocator, bucket_capacity * 2);
        }

        if (insert_internal(std::forward<K>(key), std::forward<V>(value), addr, bucket_capacity))
        {
            ++count;
        }
    }

    void remove(const Key &key)
    {
        ENGINE_ASSERT(!is_empty(), "Flat hashmap is already empty!");
        using std::swap;

        uint64_t index = Hash<Key>::Value(key) % bucket_capacity;
        Bucket *bucket = nullptr;
        uint32_t PSL = 0;
        bool is_key_found = false;

        // linear probing
        while (true)
        {
            bucket = addr + (index % bucket_capacity);
            if (bucket->occupied)
            {
                Key &occupied_key = cast_to_key(bucket);

                if (PSL > bucket->PSL)
                {
                    return;
                }

                if (occupied_key == key)
                {
                    is_key_found = true;
                    break;
                }
            }
            else
            {
                break;
            }

            ++PSL;
            ++index;
        }
        // should never go inside this
        ENGINE_ASSERT(is_key_found, "Key does not exist. Cannot remove!");
        while (true)
        {
            Bucket *next_bucket = addr + ((index + 1) % bucket_capacity);

            if (!next_bucket->occupied || next_bucket->PSL == 0)
            {
                break;
            }

            --(next_bucket->PSL);
            swap(bucket->PSL, next_bucket->PSL);
            swap(cast_to_key(bucket), cast_to_key(next_bucket));
            swap(cast_to_val(bucket), cast_to_val(next_bucket));

            ++index;
            bucket = next_bucket;
        }

        destroy_bucket(bucket);
        --count;
    }

    bool contain_key(const Key &key) const
    {
        if (is_empty())
        {
            return false;
        }

        uint64_t index = Hash<Key>::Value(key) % bucket_capacity;
        Bucket *bucket = nullptr;
        uint32_t PSL = 0;

        // linear probing
        while (true)
        {
            bucket = addr + (index % bucket_capacity);
            if (bucket->occupied)
            {
                if (PSL > bucket->PSL)
                {
                    break;
                }

                if (cast_to_key(bucket) == key)
                {
                    return true;
                }
            }
            else
            {
                break;
            }

            ++PSL;
            ++index;
        }

        return false;
    }

    Value *get_value(const Key &key)
    {
        if (is_empty())
        {
            return nullptr;
        }
        uint64_t index = Hash<Key>::Value(key) % bucket_capacity;
        Bucket *bucket = nullptr;
        uint32_t PSL = 0;

        // linear probing
        while (true)
        {
            bucket = addr + (index % bucket_capacity);
            if (bucket->occupied)
            {
                if (PSL > bucket->PSL)
                {
                    break;
                }

                if (cast_to_key(bucket) == key)
                {
                    return &cast_to_val(bucket);
                }
            }
            else
            {
                break;
            }

            PSL++;
            index++;
        }

        return nullptr;
    }

    const Value* get_value(const Key& key) const
    {
        if (is_empty())
        {
            return nullptr;
        }
        uint64_t index = Hash<Key>::Value(key) % bucket_capacity;
        Bucket *bucket = nullptr;
        uint32_t PSL = 0;

        // linear probing
        while (true)
        {
            bucket = addr + (index % bucket_capacity);
            if (bucket->occupied)
            {
                if (PSL > bucket->PSL)
                {
                    break;
                }

                if (cast_to_key(bucket) == key)
                {
                    return &cast_to_val(bucket);
                }
            }
            else
            {
                break;
            }

            PSL++;
            index++;
        }

        return nullptr;
    }

    bool is_empty() const
    {
        return count == 0;
    }

    void clear()
    {
        for (uint32_t idx = 0; idx < bucket_capacity; ++idx)
        {
            Bucket *bucket = addr + idx;
            if (bucket->occupied)
            {
                destroy_bucket(bucket);
            }
        }
        count = 0;
    }

    uint64_t get_count() const
    {
        return count;
    }
    uint64_t get_capacity() const
    {
        return bucket_capacity;
    }

    Value &operator[](const Key &key)
    {
        ENGINE_ASSERT(contain_key(key), "Key does not exist");
        return *get_value(key);
    }
    const Value &operator[](const Key &key) const
    {
        ENGINE_ASSERT(contain_key(key), "Key does not exist");
        return *get_value(key);
    }

    Iterator begin()
    {
        return Iterator(addr);
    }
    Iterator end()
    {
        return Iterator(addr + bucket_capacity);
    }

#ifdef VOID_DEBUG
    void *raw_addr() const
    {
        return addr;
    }
#endif // VOID_DEBUG
private:
    void resize(TLFS &allocator, uint64_t new_capacity)
    {
        Bucket *new_addr =
            SCAST(Bucket *, allocator.tlfs_alloc(sizeof(Bucket) * new_capacity, alignof(Bucket)));
        ENGINE_ASSERT(new_addr, "Failed to resize hashmap!");
        std::memset(new_addr, 0, sizeof(Bucket) * new_capacity);
        for (uint32_t idx = 0; idx < bucket_capacity; ++idx)
        {
            Bucket *bucket = addr + idx;
            if (bucket->occupied)
            {
                Key &key = cast_to_key(bucket);
                Value &value = cast_to_val(bucket);

                insert_internal(std::move(key), std::move(value), new_addr, new_capacity);
                destroy_bucket(bucket);
            }
        }
        allocator.tlfs_free(addr);
        bucket_capacity = new_capacity;
        addr = new_addr;
    }

    void resize(Vallocator &vallocator, uint64_t new_capacity)
    {
        Bucket *new_addr =
            SCAST(Bucket *, vallocator.valloc(sizeof(Bucket) * new_capacity, alignof(Bucket)));
        ENGINE_ASSERT(new_addr, "Failed to resize hashmap!");
        std::memset(new_addr, 0, sizeof(Bucket) * new_capacity);
        for (uint32_t idx = 0; idx < bucket_capacity; ++idx)
        {
            Bucket *bucket = addr + idx;
            if (bucket->occupied)
            {
                Key &key = cast_to_key(bucket);
                Value &value = cast_to_val(bucket);

                insert_internal(std::move(key), std::move(value), new_addr, new_capacity);
                destroy_bucket(bucket);
            }
        }
        vallocator.vfree(bucket_capacity * sizeof(Bucket), addr);
        bucket_capacity = new_capacity;
        addr = new_addr;
    }

    template <typename K, typename V>
    bool insert_internal(K &&key, V &&value, Bucket *data, uint64_t capacity)
    {
        using std::swap;

        uint64_t index = Hash<Key>::Value(key) % capacity;
        Bucket *bucket = nullptr;
        uint32_t PSL = 0;

        Key bucket_key(std::forward<K>(key));
        Value bucket_val(std::forward<V>(value));

        // linear probing
        while (true)
        {
            bucket = data + (index % capacity);
            if (!bucket->occupied)
            {
                bucket->occupied = true;
                swap(PSL, bucket->PSL);

                new (bucket->key) Key(std::move(bucket_key));
                new (bucket->value) Value(std::move(bucket_val));

                return true;
            }

            Key &key = cast_to_key(bucket);

            if (key == bucket_key)
            {
                swap(cast_to_val(bucket), bucket_val);
                return false;
            }

            if (PSL > bucket->PSL)
            {
                swap(PSL, bucket->PSL);
                swap(bucket_key, cast_to_key(bucket));
                swap(bucket_val, cast_to_val(bucket));
            }

            ++PSL;
            ++index;
        }

        return false;
    }

    void destroy_bucket(Bucket *bucket)
    {
        if constexpr (!std::is_trivially_destructible_v<Key>)
        {
            Key &key = cast_to_key(bucket);
            key.~Key();
        }

        if constexpr (!std::is_trivially_destructible_v<Value>)
        {
            Value &value = cast_to_val(bucket);
            value.~Value();
        }

        bucket->occupied = false;
        bucket->PSL = 0;
    }

    Key &cast_to_key(Bucket *bucket) const
    {
        return *RCAST(Key *, bucket->key);
    }

    Value &cast_to_val(Bucket *bucket) const
    {
        return *RCAST(Value *, bucket->value);
    }

};
} // namespace VoidEngine
