#pragma once
#include "ds/world_allocator.h"

/*
    Robin hood open addressing hash map
*/

namespace ECS
{
    //chatgpt
    inline uint64_t HashU64(uint64_t x)
    {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;

        return x;
    }

    template<typename T>
    struct Hash;

    template<>
    struct Hash<uint64_t>
    {
        static uint64_t Value(uint64_t v)
        {
            return HashU64(v);
        }
    };

    template<>
    struct Hash<uint32_t>
    {
        static uint64_t Value(uint32_t v)
        {
            return HashU64((uint64_t) v);
        }
    };

    template<typename Key, typename Value>
    class HashMap
    {

    private:
        struct Bucket
        {
            uint32_t PSL;
            uint8_t occupied;
            unsigned char padding[3];
            alignas(Key) char key[sizeof(Key)];
            alignas(Value) char value[sizeof(Value)];
        };

    public:
        struct Iterator
        {
        private:
            friend class HashMap;

            Bucket* m_ptr;
        private:
            Iterator(Bucket* ptr)
                : m_ptr(ptr)
            {
            }

        public:

            bool IsValid() const
            {
                return m_ptr->occupied;
            }

            Value& GetValue()
            {
                return *PTR_RCAST(m_ptr->value, Value);
            }

            Key& GetKey()
            {
                return *PTR_RCAST(m_ptr->key, Key);
            }

            const Value& GetValue() const
            {
                return *PTR_RCAST(m_ptr->value, Value);
            }

            const Key& GetKey() const
            {
                return *PTR_RCAST(m_ptr->key, Key);
            }

            Iterator& operator++()
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

            Iterator& operator--()
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

            bool operator==(const Iterator& other) const
            {
                return m_ptr == other.m_ptr;
            }

            bool operator!=(const Iterator& other) const
            {
                return m_ptr != other.m_ptr;
            }
        };

    public:
        HashMap() :
            m_array(nullptr), m_allocator(nullptr),
            m_bucketCount(0), m_count(0)
        {
            static_assert(std::is_move_constructible_v<Key> || std::is_copy_constructible_v<Key>);
            static_assert(std::is_move_constructible_v<Value> || std::is_copy_constructible_v<Value>);
            static_assert(std::is_move_assignable_v<Value> || std::is_copy_assignable_v<Value>);
            static_assert(std::is_move_assignable_v<Key> || std::is_copy_assignable_v<Key>);
            static_assert(std::is_destructible_v<Key> && std::is_destructible_v<Value>);
        }

        void Init(WorldAllocator* allocator, uint32_t bucketCount)
        {
            m_allocator = allocator;
            m_bucketCount = bucketCount;
            m_count = 0;
            m_array = CallocN(m_bucketCount);
        }

        void Destroy()
        {
            for(uint32_t i = 0; i < m_bucketCount; i++)
            {
                Bucket* bucket = CAST_OFFSET_ELEMENT(m_array, Bucket, sizeof(Bucket), i);

                assert(bucket && "Bucket is null!");

                if(bucket->occupied)
                {
                    Key& key = KeyCast(bucket);
                    Value& value = ValueCast(bucket);

                    DestroyBucket(bucket);
                }
            }

            if(m_allocator)
            {
                m_allocator->Free(m_alignedBucketSize * m_bucketCount, m_array);
            }
            else
            {
                std::free(m_array);
            }
        }

        template<typename K, typename V>
        void Insert(K&& key, V&& value)
        {
            if(static_cast<float>(m_count) / static_cast<float>(m_bucketCount) >= 0.8)
            {
                Grow();
            }

            if(InsertInternal(std::forward<K>(key), std::forward<V>(value), m_array, m_bucketCount))
            {
                ++m_count;
            }
        }


        void Remove(const Key& key)
        {
            size_t index = Hash<Key>::Value(key) % m_bucketCount;
            Bucket* bucket = nullptr;
            uint32_t PSL = 0;
            bool isKeyFound = false;

            //linear probing
            while(true)
            {
                bucket = CAST_OFFSET_ELEMENT(m_array, Bucket, sizeof(Bucket), (index % m_bucketCount));
                if(bucket->occupied)
                {
                    Key& occupiedKey = KeyCast(bucket);

                    if(PSL > bucket->PSL)
                    {
                        return;
                    }

                    if(occupiedKey == key)
                    {
                        isKeyFound = true;
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

            //should never go inside this
            if(!isKeyFound)
            {
                return;
            }

            while(true)
            {
                Bucket* nextBucket = CAST_OFFSET_ELEMENT(m_array, Bucket, sizeof(Bucket), ((index + 1) % m_bucketCount));

                if(!nextBucket->occupied || nextBucket->PSL == 0)
                {
                    break;
                }

                --(nextBucket->PSL);
                std::swap(bucket->PSL, nextBucket->PSL);
                std::swap(KeyCast(bucket), KeyCast(nextBucket));
                std::swap(ValueCast(bucket), ValueCast(nextBucket));

                ++index;
                bucket = nextBucket;
            }

            DestroyBucket(bucket);
            --m_count;
        }

        bool ContainsKey(const Key& key) const
        {
            size_t index = Hash<Key>::Value(key) % m_bucketCount;
            Bucket* bucket = nullptr;
            uint32_t PSL = 0;

            //linear probing
            while(true)
            {
                bucket = CAST_OFFSET_ELEMENT(m_array, Bucket, sizeof(Bucket), (index % m_bucketCount));
                if(bucket->occupied)
                {
                    if(PSL > bucket->PSL)
                    {
                        break;
                    }

                    if(KeyCast(bucket) == key)
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

        Value& GetValue(const Key& key)
        {
            size_t index = Hash<Key>::Value(key) % m_bucketCount;
            Bucket* bucket = nullptr;
            uint32_t PSL = 0;

            //linear probing
            while(true)
            {
                bucket = CAST_OFFSET_ELEMENT(m_array, Bucket, sizeof(Bucket), (index % m_bucketCount));
                if(bucket->occupied)
                {
                    if(PSL > bucket->PSL)
                    {
                        break;
                    }

                    if(KeyCast(bucket) == key)
                    {
                        return ValueCast(bucket);
                    }
                }
                else
                {
                    break;
                }

                PSL++;
                index++;
            }

            assert(0 && "No key exist!");
        }

        bool Empty() const
        {
            return m_count == 0;
        }

        size_t GetCount() const
        {
            return m_count;
        }

        size_t GetBucketCount() const
        {
            return m_bucketCount;
        }

        Value& operator[](const Key& key)
        {
            return GetValue(key);
        }

        Iterator Begin()
        {
            return Iterator(RCAST(m_array, Bucket));
        }

        Iterator End()
        {
            return Iterator(CAST_OFFSET_ELEMENT(m_array, Bucket, sizeof(Bucket), m_bucketCount));
        }

    private:
        void* CallocN(uint32_t capacity)
        {
            void* data = nullptr;
            if(m_allocator)
            {
                uint32_t newCapacity = 0;
                data = m_allocator->CallocN(sizeof(Bucket), capacity, newCapacity);
                m_bucketCount = newCapacity;
            }
            else
            {
                data = std::calloc(m_bucketCount, sizeof(Bucket));
            }

            assert(data && "Fail to calloc!");

            return data;
        }

        void Grow()
        {
            uint32_t newBucketCount = m_bucketCount * 2;

            void* newArray = CallocN(newBucketCount);

            for(uint32_t i = 0; i < m_bucketCount; i++)
            {
                Bucket* bucket = CAST_OFFSET_ELEMENT(m_array, Bucket, sizeof(Bucket), i);

                assert(bucket && "Bucket is null!");

                if(bucket->occupied)
                {
                    Key& key = KeyCast(bucket);
                    Value& value = ValueCast(bucket);

                    InsertInternal(std::move(key), std::move(value), newArray, newBucketCount);

                    DestroyBucket(bucket);
                }
            }

            if(m_allocator)
            {
                m_allocator->Free(sizeof(Bucket) * m_bucketCount, m_array);
            }

            m_bucketCount = newBucketCount;
            m_array = newArray;
        }

        template<typename K, typename V>
        bool InsertInternal(K&& key, V&& value, void* array, size_t bucketCount)
        {
            size_t index = Hash<Key>::Value(key) % bucketCount;
            Bucket* bucket = nullptr;
            uint32_t PSL = 0;

            Key bucketKey(std::forward<K>(key));
            Value bucketValue(std::forward<V>(value));

            //linear probing
            while(true)
            {
                bucket = CAST_OFFSET_ELEMENT(array, Bucket, sizeof(Bucket), (index % bucketCount));
                if(!bucket->occupied)
                {
                    bucket->occupied = true;
                    std::swap(PSL, bucket->PSL);

                    new (bucket->key) Key(std::move(bucketKey));
                    new (bucket->value) Value(std::move(bucketValue));

                    return true;
                }

                Key& key = KeyCast(bucket);

                if(key == bucketKey)
                {
                    std::swap(ValueCast(bucket), bucketValue);
                    return false;
                }

                if(PSL > bucket->PSL)
                {
                    std::swap(PSL, bucket->PSL);
                    std::swap(bucketKey, KeyCast(bucket));
                    std::swap(bucketValue, ValueCast(bucket));
                }

                ++PSL;
                ++index;
            }

            return false;
        }

        void DestroyBucket(Bucket* bucket)
        {
            if constexpr(!std::is_trivially_destructible_v<Key>)
            {
                Key& key = KeyCast(bucket);
                key.~Key();
            }

            if constexpr(!std::is_trivially_destructible_v<Value>)
            {
                Value& value = ValueCast(bucket);
                value.~Value();
            }            

            bucket->occupied = false;
            bucket->PSL = 0;
        }

        Key& KeyCast(Bucket* bucket) const
        {
            return *PTR_RCAST(bucket->key, Key);
        }

        Value& ValueCast(Bucket* bucket) const
        {
            return *PTR_RCAST(bucket->value, Value);
        }

    private:
        void* m_array;
        WorldAllocator* m_allocator;
        uint32_t m_bucketCount;
        uint32_t m_count;
    };
}