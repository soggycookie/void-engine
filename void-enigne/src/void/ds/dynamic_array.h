#pragma once
#include "void/pch.h"
#include "void/allocator/free_list_allocator.h"

//THIS IS NOT FOR GENERIC USE
//IT CAN ONLY SUPPORT TRIVIALLY COPYABLE DATA TYPE
//BASICALLY PRIMITIVE TYPE AND POD STRUCT

namespace VoidEngine
{

#define DEFAULT_CAPACITY 10

    template<typename T>
    class DynamicArray
    {
    public:
        struct Iterator
        {
        private:
            friend class DynamicArray;

            T* m_ptr;

        private:
            Iterator(T* ptr)
                : m_ptr(ptr)
            {
            }
        
        public:

            T& operator*() noexcept
            {
                return *m_ptr;
            }

            const T& operator*() const noexcept
            {
                return *m_ptr;
            }

            T* operator->() noexcept
            {
                return m_ptr;
            }
            
            const T* operator->() const noexcept
            {
                return m_ptr;
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
        DynamicArray()
            : m_allocator(nullptr), m_alignedSize(0), 
            m_arrBytes(0), m_count(0), m_capacity(0)
        {
        }

        DynamicArray(FreeListAllocator* allocator, size_t capacity,const T& defaultValue)
            : m_allocator(allocator), m_alignedSize(0), 
            m_arrBytes(0), m_count(0), m_capacity(capacity)
        {
            size_t align = alignof(T);
            size_t mask = align - 1;
            size_t size = sizeof(T);

            static_assert(std::is_destructible_v<T>, "Type must be destructible! [DynamicArray]");

            if(m_capacity <= 0)
            {
                assert(0 && "Capacity can not be equal or smaller than 0! [DynamicArray]");
            }

            m_alignedSize = (size + mask) & ~mask;

            m_arrBytes = m_alignedSize * capacity;

            m_data = static_cast<uint8_t*>(allocator->Alloc(m_arrBytes, align));

            for(size_t i = 0; i < m_capacity ; i++)
            {
                //T* element = reinterpret_cast<T*>(m_data + i * m_alignedSize);
                void* element = (m_data + i * m_alignedSize);
                new (element) T(defaultValue);
            }
        }

        DynamicArray(FreeListAllocator* allocator, size_t capacity)
            : m_allocator(allocator), m_alignedSize(0), 
            m_arrBytes(0), m_count(0), m_capacity(capacity)
        {
            size_t align = alignof(T);
            size_t mask = align - 1;
            size_t size = sizeof(T);

            if(m_capacity <= 0)
            {
                assert(0 && "Capacity can not be equal or smaller than 0! [DynamicArray]");
            }

            m_alignedSize = (size + mask) & ~mask;

            m_arrBytes = m_alignedSize * capacity;

            m_data = static_cast<uint8_t*>(allocator->Alloc(m_arrBytes, align));
        }

        ~DynamicArray()
        {
            if(m_allocator)
            {
                if(m_data)
                {
                    m_allocator->Free(m_data);
                }
            }
        }

        DynamicArray& operator=(DynamicArray&& arr)
        {
            if(m_data && m_allocator)
            {
                m_allocator->Free(m_data);
            }

            m_data          = arr.m_data;
            m_allocator     = arr.m_allocator;
            m_capacity      = arr.m_capacity;
            m_count         = arr.m_count;
            m_alignedSize   = arr.m_alignedSize;
            m_arrBytes      = arr.m_arrBytes;

            arr.m_data          = nullptr;
            arr.m_allocator     = nullptr;
            arr.m_capacity      = 0;
            arr.m_count         = 0;
            arr.m_alignedSize   = 0;
            arr.m_arrBytes      = 0;
            
        }

        void Resize(size_t capacity)
        {
                
        }

        void PushBack(const T& value)
        {
            if(m_count >= m_capacity)
            {
                Resize(100);
            }

            T* element = reinterpret_cast<T*>(m_data + m_count * m_alignedSize);
            m_count++;

            *element = value;
        }

        template<typename... Args>
        void EmplaceBack(Args&&... args)
        {
            if(m_count >= m_capacity)
            {
                Resize(100);
            }

            void* addr = (m_data + m_count * m_alignedSize);
            new (addr) T(std::forward<Args>(args)...);         

            m_count++;
        }

        template<typename... Args>
        void Emplace(Iterator emplacedIt, Args&&... args)
        {
            if(m_count >= m_capacity)
            {
                Resize(100);
            }

            if (emplacedIt != End()) {
                // Move construct at the end
                new (m_data + m_count * m_alignedSize) T(std::move(*(End() - 1)));
        
                // Move elements backward
                for(Iterator it = End() - 1; it != emplacedIt; it--) {
                    *it = std::move(*(it - 1));
                }
        
                // Destroy the old object at emplacedIt
                if constexpr(std::is_destructible_v<T>)
                {
                    (*emplacedIt).~T();
                }
            }
    
            // Construct new object in-place at emplacedIt
 
            new (emplacedIt.m_ptr) T(std::forward<Args>(args)...);
            

            m_count++;
        }

        void Insert(Iterator emplacedIt, const T& value)
        {
            if(m_count >= m_capacity)
            {
                Resize(100);
            }

            if (emplacedIt != End()) {
                // Move construct at the end
                new (m_data + m_count * m_alignedSize) T(std::move(*(End() - 1)));
        
                // Move elements backward
                for(Iterator it = End() - 1; it != emplacedIt; it--) {
                    *it = std::move(*(it - 1));
                }
        
                // Destroy the old object at emplacedIt
                if constexpr(std::is_destructible_v<T>)
                {
                    (*emplacedIt).~T();
                }
            }
    
            // Construct new object in-place at emplacedIt
 
            new (emplacedIt.m_ptr) T(value);

            m_count++;
        }

        

        void Remove(Iterator it)
        {
            if(it == End())
            {
                //log
                return;
            }

            T* movedAddr = it.m_ptr + 1;
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_data + m_count * m_alignedSize);
            uintptr_t removedAddr = reinterpret_cast<uintptr_t>(movedAddr);
            size_t moveSize = (endAddr - removedAddr) ;

            std::memmove(it.m_ptr, movedAddr, moveSize);
            m_count--;
        }

        void RemoveAt(size_t index)
        {
            if(index >= m_count)
            {
                return;
            }

            size_t moveCount = m_count - index - 1;

            std::memmove(m_data + index * m_alignedSize, m_data + (index + 1) * m_alignedSize, moveCount * m_alignedSize);
            m_count--;
        }

        Iterator Find(const T& value)
        {
            for(auto it = Begin(); it != End(); it++)
            {
                if(*it == value)
                {
                    return it;
                }
            }

            return End();
        }

        size_t GetCount() const
        {
            return m_count;
        }

        T& operator[](size_t index)
        {
            if(index >= m_count)
            {
                assert(0 && "Access out of bound index [DynamicArray]");                
            }

            T* element = reinterpret_cast<T*>(m_data + index * m_alignedSize); 

            return *element;
        }

        Iterator Begin()
        {
            return Iterator(reinterpret_cast<T*>(m_data));
        }

        Iterator End()
        {
            return Iterator(reinterpret_cast<T*>(m_data + m_count * m_alignedSize));
        }

    private:
        FreeListAllocator* m_allocator;
        uint8_t* m_data;
        size_t m_capacity;
        size_t m_count;
        size_t m_alignedSize;
        size_t m_arrBytes;
    };

}
