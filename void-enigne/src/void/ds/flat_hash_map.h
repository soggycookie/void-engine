#pragma once
#include "void/pch.h"
#include "void/allocator/free_list_allocator.h"

//THIS IS NOT FOR GENERIC USE
//KEY MUST HAVE COPY/MOVE CONSTRUCTOR
//VALUE IS POD

namespace VoidEngine
{

#define DEFAULT_BUCKET 16

    template<typename Key, typename Value>
    class FlatHashMap
    {

    private:

        struct BucketHeader
        {
            uint32_t PSL;
            uint8_t occupied;
            unsigned char padding[3];
        };
        
        struct Bucket
        {
            BucketHeader header;
            alignas(Key) unsigned char key[sizeof(Key)];
            alignas(Value) unsigned char value[sizeof(Value)];
        };
    
    public:
        struct Iterator
        {
        private:
            friend class FlatHashMap;

            Bucket* m_ptr;
        private:
            Iterator(Bucket* ptr)
                : m_ptr(ptr)
            {
            }

        public:

            bool IsValid() const
            {
                return m_ptr->header.occupied;
            }

            Value& GetValue()
            {
                return *(reinterpret_cast<Value*>(m_ptr->value));
            }

            Key& GetKey()
            {
                return *(reinterpret_cast<Key*>(m_ptr->key));
            }
            
            const Value& GetValue() const
            {
                return *(reinterpret_cast<Value*>(m_ptr->value));
            }

            const Key& GetKey() const
            {
                return *(reinterpret_cast<Key*>(m_ptr->key));
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
        FlatHashMap() = default;
        
        FlatHashMap(FreeListAllocator* allocator)
            : FlatHashMap(allocator, DEFAULT_BUCKET)
        {
        }

        FlatHashMap(FreeListAllocator* allocator, size_t bucketCount)
            : m_allocator(allocator), m_bucketCount(bucketCount), m_count(0)
        {
            static_assert((std::is_move_constructible_v<Key> || std::is_copy_constructible_v<Key>)
                          && std::is_destructible_v<Key>, "Key must be movable or copyable! [FlatHashMap]");
            static_assert(std::is_trivially_copyable_v<Value> 
                          && std::is_move_constructible_v<Value>
                          && std::is_trivially_destructible_v<Value>, "Value must be POD only! [FlatHashMap]");

            size_t bucketSize = sizeof(Bucket);
            size_t bucketAlign = alignof(Bucket);
            size_t mask = bucketAlign - 1;

            m_alignedBucketSize = (bucketSize + mask) & ~mask;

            m_data = static_cast<uint8_t*>(allocator->Alloc(m_alignedBucketSize * m_bucketCount, bucketAlign));

            if(!m_data)
            {
                assert(0 && "FlatHashMap failed to alloc! [FlatHashMap]");
            }
        }

        FlatHashMap(FlatHashMap&& map)
        {
            if(this == &map)
            {
                return;
            }

            if(!m_data)
            {
                m_allocator->Free(m_data);
                m_data = map.m_data;
                map.m_data = nullptr;
            }

            if(!map.m_allocator)
            {
                m_allocator = map.m_allocator;
                map.m_allocator = nullptr;
            }

            m_count = map.m_count;
            m_bucketCount = map.m_bucketCount;
            m_alignedBucketSize = map.m_alignedBucketSize;

            map.m_count = 0;
            map.m_bucketCount = 0;
            map.m_alignedBucketSize = 0;           
        }

        ~FlatHashMap()
        {
            if(m_data && m_allocator)
            {
                m_allocator->Free(m_data);
            }
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

        bool ContainsKey(const Key& key) const
        {
            size_t index = Hash(key) & (m_bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedBucketSize);
            size_t PSL = 0;

            //linear probing
            while(true)
            {
                if(bucket->header.occupied)
                {
                    if(PSL > bucket->header.PSL)
                    {
                        break;
                    }

                    if((*KeyCast(bucket)) == key)
                    {
                        return true;
                    }
                }
                else
                {
                    break;
                }

                PSL++;
                index++;
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedBucketSize);
            }

            return false;
        }

        Value* GetValue(const Key& key)
        {
            size_t index = Hash(key) & (m_bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedBucketSize);
            size_t PSL = 0;

            //linear probing
            while(true)
            {
                if(bucket->header.occupied)
                {
                    if(PSL > bucket->header.PSL)
                    {
                        break;
                    }

                    if((*KeyCast(bucket)) == key)
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
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedBucketSize);
            }

            return nullptr;
        }

        void Insert(Key&& key, Value&& value)
        {
            if(m_count >= (m_bucketCount * 0.8f))
            {
                if(!Resize(m_bucketCount << 1))
                {
                    assert(0 && "[FlatHashMap] Failed to resize the map!");
                    return;
                }
            }

            if(Insert(std::move(key), std::move(value), m_data, m_bucketCount))
            {
                m_count++;
            }
        }

        void Insert(const Key& key, const Value& value)
        {
            if(m_count >= (m_bucketCount * 0.8f))
            {
                if(!Resize(m_bucketCount << 1))
                {
                    assert(0 && "[FlatHashMap] Failed to resize the map!");
                    return;
                }
            }

            if(Insert(key, value, m_data, m_bucketCount))
            {
                m_count++;
            }
        }

        //NEVER USE THIS IN ITERAION LOOP
        void Remove(const Key& key)
        {
            size_t index = Hash(key) & (m_bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedBucketSize);
            size_t PSL = 0;
            bool isKeyFound = false;

            //linear probing
            while(true)
            {
                if(bucket->header.occupied)
                {
                    Key* occupiedKey = KeyCast(bucket);

                    if(PSL > bucket->header.PSL)
                    {
                        assert(0 && "Key does not exist! [FlatHashMap.Remove]");
                        return;
                    }

                    if(*occupiedKey == key)
                    {
                        isKeyFound = true;
                        break;
                    }
                }
                else
                {
                    break;
                }

                PSL++;
                index++;
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedBucketSize);
            }

            //should never go inside this
            if(!isKeyFound)
            {
                return;
            }

            while(true)
            {
                Bucket* nextBucket = reinterpret_cast<Bucket*>(m_data + ((index + 1) & (m_bucketCount - 1)) * m_alignedBucketSize);
                
                if(!nextBucket->header.occupied || nextBucket->header.PSL == 0)
                {
                    break;
                }

                std::swap(bucket, nextBucket);
                bucket->header.PSL--;

                std::cout << bucket->key << ": " << bucket->header.PSL << std::endl;

                index++;
                bucket = nextBucket;
            }

            KeyCast(bucket)->~Key();
            ValueCast(bucket)->~Value();
        }

        Value& operator[](const Key& key)
        {
            Value bucketValue = {};

            Value* value = GetValue(key);
            if(value)
            {
                return *value;
            }

            Insert(key, bucketValue);

            return *GetValue(key);
        }
        
        FlatHashMap& operator=(FlatHashMap&& map) noexcept
        {
            if(this == &map)
            {
                return *this;
            }

            if(m_data && m_allocator)
            {
                m_allocator->Free(m_data);
            }


            m_allocator = map.m_allocator;
            m_data = map.m_data;
            m_count = map.m_count;
            m_bucketCount = map.m_bucketCount;
            m_alignedBucketSize = map.m_alignedBucketSize;

            map.m_data = nullptr;
            map.m_allocator = nullptr;
            map.m_count = 0;
            map.m_bucketCount = 0;
            map.m_alignedBucketSize = 0; 

            return *this;
        }

        Iterator Begin()
        {
            return Iterator(reinterpret_cast<Bucket*>(m_data));
        }

        Iterator End()
        {
            return Iterator(reinterpret_cast<Bucket*>(m_data + m_bucketCount * m_alignedBucketSize));
        }

private:

        bool Insert(Key&& key, Value&& value, uint8_t* data, size_t bucketCount)
        {
            size_t index = Hash(key) & (bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(data + index * m_alignedBucketSize);
            uint32_t PSL = 0;
            
            Key bucketKey(std::move(key));
            Value bucketValue = value;


            //linear probing
            while(true)
            {
                if(!bucket->header.occupied)
                {
                    bucket->header.occupied = true;
                    std::swap(PSL, bucket->header.PSL);
                    *(ValueCast(bucket)) = bucketValue;
                    //std::swap(bucketValue, bucket->value);
                    
                    new (bucket->key) Key(std::move(bucketKey));
                    
                    return true;
                }

                Key* key = KeyCast(bucket);

                if(*key == bucketKey && m_data == data)
                {
                    assert(0 && "Key already existed! [FlatHashMap]");
                    return false;
                }

                if(PSL > bucket->header.PSL)
                {
                    //SIMPLE_LOG("SWAP");
                    std::swap(PSL, bucket->header.PSL);
                    Value* val = ValueCast(bucket);
                    Value temp = *val;

                    *val = bucketValue;
                    bucketValue = temp;

                    std::swap(bucketKey, *key);
                }

                PSL++;
                index++;
                bucket = reinterpret_cast<Bucket*>(data + (index & (bucketCount - 1)) * m_alignedBucketSize);
            }

            return false;
        }

        bool Insert(const Key& key, const Value& value, uint8_t* data, size_t bucketCount)
        {
            size_t index = Hash(key) & (bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(data + index * m_alignedBucketSize);
            uint32_t PSL = 0;
            
            Key bucketKey(key);
            Value bucketValue(value);

            //linear probing
            while(true)
            {
                if(!bucket->header.occupied)
                {
                    bucket->header.occupied = true;
                    std::swap(PSL, bucket->header.PSL);
                    *(ValueCast(bucket)) = bucketValue;
                    //std::swap(bucketValue, bucket->value);

                    new (bucket->key) Key(bucketKey);
                    
                    return true;
                }

                Key* key = KeyCast(bucket);

                if(*key == bucketKey && m_data == data)
                {
                    assert(0 && "Key already existed! [FlatHashMap]");
                    return false;
                }

                if(PSL > bucket->header.PSL)
                {
                    //SIMPLE_LOG("SWAP");
                    std::swap(PSL, bucket->header.PSL);
                    Value* val = ValueCast(bucket);
                    Value temp = *val;

                    *val = bucketValue;
                    bucketValue = temp;

                    std::swap(bucketKey, *key);
                }

                PSL++;
                index++;
                bucket = reinterpret_cast<Bucket*>(data + (index & (bucketCount - 1)) * m_alignedBucketSize);
            }

            return false;
        }

        bool Resize(size_t newBucketCount)
        {
            std::cout << "resize" << std::endl;
            uint8_t* newData = 
                static_cast<uint8_t*>(m_allocator->Alloc(newBucketCount * m_alignedBucketSize));
            
            if(!newData)
            {
                return false;
            }

            for(Iterator it = Begin(); it != End(); it++)
            {
                if(it.IsValid())
                {
                    Key& key = it.GetKey();
                    Value& value = it.GetValue();
                    if constexpr(std::is_move_constructible_v<Key>)
                    {
                        Insert(std::move(key), std::move(value), newData, newBucketCount);
                    }
                    else
                    {
                        Insert(key, value, newData, newBucketCount);
                    }

                    key.~Key();
                    value.~Value();
                }
            }

            m_allocator->Free(m_data);
            m_data = newData;
            m_bucketCount = newBucketCount;

            return true;
        }

        Key* KeyCast(Bucket* bucket) const
        {
            return reinterpret_cast<Key*>(bucket->key);
        }
        
        Value* ValueCast(Bucket* bucket) const
        {
            return reinterpret_cast<Value*>(bucket->value);
        }

        //chatgpt :0
        static size_t HashMix(size_t x) {
            x ^= x >> 33;
            x *= 0xff51afd7ed558ccdULL;
            x ^= x >> 33;
            x *= 0xc4ceb9fe1a85ec53ULL;
            x ^= x >> 33;
            return x;
        }

        static size_t Hash(const Key& key)
        {
            size_t hash = std::hash<Key>{}(key);
            return HashMix(hash);
        }

    private:
        uint8_t* m_data;
        FreeListAllocator* m_allocator;    

        size_t m_count;
        size_t m_bucketCount;
        //size_t m_maxPSL;
        size_t m_alignedBucketSize;
    };

}