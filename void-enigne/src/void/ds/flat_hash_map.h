#pragma once
#include "void/pch.h"
#include "void/allocator/free_list_allocator.h"

//THIS IS NOT FOR GENERIC USE
//KEY MUST HAVE COPY/MOVE CONSTRUCTOR
//VALUE IS POD

namespace VoidEngine
{

#define DEFAULT_BUCKET 32

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

            size_t nodeSize = sizeof(Bucket);
            size_t nodeAlign = alignof(Bucket);
            size_t mask = nodeAlign - 1;

            m_alignedNodeSize = (nodeSize + mask) & ~mask;

            m_data = static_cast<uint8_t*>(allocator->Alloc(m_alignedNodeSize * m_bucketCount, nodeAlign));

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
            m_alignedNodeSize = map.m_alignedNodeSize;

            map.m_count = 0;
            map.m_bucketCount = 0;
            map.m_alignedNodeSize = 0;           
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

        bool ContainsKey(const Key& key) const
        {
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_data);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_data + m_bucketCount * m_alignedNodeSize);

            size_t index = Hash(key) & (m_bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedNodeSize);
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

                    if(*(reinterpret_cast<Key*>(bucket->key)) == key)
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
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedNodeSize);
            }

            return false;
        }

        bool Insert(Key&& key, Value&& value)
        {
            if(m_count == m_bucketCount)
            {
                Resize(m_bucketCount << 1);
            }

            size_t index = Hash(key) & (m_bucketCount - 1);
            SIMPLE_LOG("index: ");
            SIMPLE_LOG(index);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedNodeSize);
            uint32_t PSL = 0;
            
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_data);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_data + m_bucketCount * m_alignedNodeSize);
            
            Key bucketKey(std::move(key));
            Value bucketValue = value;


            //linear probing
            while(true)
            {
                if(!bucket->header.occupied)
                {
                    bucket->header.occupied = true;
                    std::swap(PSL, bucket->header.PSL);
                    *(reinterpret_cast<Value*>(bucket->value)) = bucketValue;
                    //std::swap(bucketValue, bucket->value);
                    
                    if constexpr (std::is_trivially_constructible_v<Key> &&
                                  std::is_trivially_copyable_v<Key>) {
                        new (&bucket->key) Key(bucketKey);
                    } else {
                        new (&bucket->key) Key(std::move(bucketKey));
                    }

                    m_count++;
                    return true;
                }
                Key* key = reinterpret_cast<Key*>(bucket->key);
                if(*key == bucketKey)
                {
                    assert(0 && "Key already existed! [FlatHashMap]");
                    return false;
                }

                if(PSL > bucket->header.PSL)
                {
                    //SIMPLE_LOG("SWAP");
                    std::swap(PSL, bucket->header.PSL);
                    Value* val = reinterpret_cast<Value*>(bucket->value);
                    Value temp = *val;

                    *val = bucketValue;
                    bucketValue = temp;

                    //if constexpr(std::is_move_constructible_v<Key> || std::is_copy_constructible_v<Key>)
                    //{
                    std::swap(bucketKey, *key);
                    //}
                }

                PSL++;
                index++;
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedNodeSize);
            }

            return false;
        }

        bool Insert(const Key& key, const Value& value)
        {
            if(m_count == m_bucketCount)
            {
                Resize(m_bucketCount << 1);
            }

            size_t index = Hash(key) & (m_bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedNodeSize);
            uint32_t PSL = 0;
            
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_data);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_data + m_bucketCount * m_alignedNodeSize);
            
            Key bucketKey(std::move(key));
            Value bucketValue = value;


            //linear probing
            while(true)
            {
                if(!bucket->header.occupied)
                {
                    bucket->header.occupied = true;
                    std::swap(PSL, bucket->header.PSL);
                    new (bucket->value) Value(bucketValue); 
                    
                    if constexpr (std::is_trivially_constructible_v<Key> &&
                                  std::is_trivially_copyable_v<Key>) {
                        new (&bucket->key) Key(bucketKey);
                    } else {
                        new (&bucket->key) Key(std::move(bucketKey));
                    }

                    m_count++;
                    return true;
                }

                Key* key = KeyCast(bucket);

                if( *key == bucketKey)
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

                    //if constexpr(std::is_move_constructible_v<Key> || std::is_copy_constructible_v<Key>)
                    //{
                    std::swap(bucketKey, *key);
                    //}
                }

                PSL++;
                index++;
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedNodeSize);
            }

            return false;
        }

        //NEVER USE THIS IN ITERAION LOOP
        void Remove(const Key& key)
        {
            size_t index = Hash(key) & (m_bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedNodeSize);
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
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedNodeSize);
            }

            //should never go inside this
            if(!isKeyFound)
            {
                return;
            }

            while(true)
            {
                Bucket* nextBucket = reinterpret_cast<Bucket*>(m_data + ((index + 1) & (m_bucketCount - 1)) * m_alignedNodeSize);
                
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
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_data);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_data + m_bucketCount * m_alignedNodeSize);

            size_t index = Hash(key) & (m_bucketCount - 1);
            Bucket* bucket = reinterpret_cast<Bucket*>(m_data + index * m_alignedNodeSize);
            uint32_t PSL = 0;
            
            Key bucketKey;
            Value bucketValue = {};
            bucketKey = std::move(key);
            bool isAdded = false;
            Bucket* addedBucket = nullptr;
            
            //linear probing
            while(true)
            {
                if(!bucket->header.occupied)
                {
                    bucket->header.occupied = true;
                    std::swap(PSL, bucket->header.PSL);
                    *(reinterpret_cast<Value*>(bucket->value)) = bucketValue;
                    
                    if constexpr (std::is_trivially_constructible_v<Key> &&
                                  std::is_trivially_copyable_v<Key>) {
                        new (&bucket->key) Key(bucketKey);
                    } else {
                        new (&bucket->key) Key(std::move(bucketKey));
                    }

                    if(!isAdded)
                    {
                        isAdded = !isAdded;
                        addedBucket = bucket;
                    }

                    m_count++;
                    break;
                }
                Key* key = KeyCast(bucket);

                if(*key == bucketKey)
                {
                    return *ValueCast(bucket);
                }

                if(PSL > bucket->header.PSL)
                {
                    std::swap(PSL, bucket->header.PSL);
                    Value* val = ValueCast(bucket);
                    Value temp = *val;
                    *val = bucketValue;
                    bucketValue = temp;

                    std::swap(bucketKey, *key);

                    if(!isAdded)
                    {
                        isAdded = !isAdded;
                        addedBucket = bucket;
                    }
                }

                PSL++;
                index++;
                bucket = reinterpret_cast<Bucket*>(m_data + (index & (m_bucketCount - 1)) * m_alignedNodeSize);
            }


            return *ValueCast(addedBucket);
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
            m_alignedNodeSize = map.m_alignedNodeSize;

            map.m_data = nullptr;
            map.m_allocator = nullptr;
            map.m_count = 0;
            map.m_bucketCount = 0;
            map.m_alignedNodeSize = 0; 

            return *this;
        }

        Iterator Begin()
        {
            return Iterator(reinterpret_cast<Bucket*>(m_data));
        }

        Iterator End()
        {
            return Iterator(reinterpret_cast<Bucket*>(m_data + m_bucketCount * m_alignedNodeSize));
        }

private:
        bool Resize(size_t newBucketCount)
        {
            
            return true;
        }

        Key* KeyCast(Bucket* bucket)
        {
            return reinterpret_cast<Key*>(bucket->key);
        }
        
        Value* ValueCast(Bucket* bucket)
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
        size_t m_alignedNodeSize;
    };

}