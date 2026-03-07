#pragma once
#include "ecs_pch.h"
#include "ds/hash_map.h"


template<typename T>
struct ComponentName;

#define ECS_COMPONENT(T) \
        template<> \
        struct ComponentName<T> \
        { \
            static constexpr const char* name = #T; \
        }; 

namespace ECS
{

#define ENTITY_ID_MASK      0xFFFFFFFFULL
#define ENTITY_GEN_MASK     0xFFFFULL

#define LO_ENTITY_ID(x) \
    ((uint32_t)((x) & ENTITY_ID_MASK))

#define HI_ENTITY_ID(x) \
    ((uint32_t)(((x) >> 32) & ENTITY_ID_MASK))

#define ENTITY_GEN_COUNT(x) \
    ((uint16_t)(((x) >> 32) & ENTITY_GEN_MASK))

#define MAKE_ENTITY_ID(lo, hi) \
    ((((uint64_t)(hi) & ENTITY_ID_MASK) << 32) | \
     ((uint64_t)(lo)  & ENTITY_ID_MASK))

#define INCRE_GEN_COUNT(x) \
        MAKE_ENTITY_ID( \
            LO_ENTITY_ID(x), \
            (uint16_t)(ENTITY_GEN_COUNT(x) + 1) \
        )

    using EntityId = uint64_t;
    using LoEntityId = uint32_t;
    using HiEntityId = uint32_t;
    using GenCount = uint16_t;

    inline EntityId MakeRelationship(EntityId first, EntityId sec)
    {
        constexpr uint32_t mask = 0xFFFFFFFFULL;

        LoEntityId f = first & mask;
        LoEntityId s = sec & mask;

        EntityId pair = (EntityId) f | ((EntityId)s << 32);

        return pair;
    }

    //inline EntityId GetNextComponentId()
    //{
    //    static EntityId id = 0;
    //    return id++;
    //}


    //template<typename T>
    //EntityId GetComponentId()
    //{
    //    static EntityId id = GetNextComponentId();
    //    return id;
    //}

    class World;
    template<typename T>
    struct ComponentTypeId
    {
        static EntityId id;

        static void Id(EntityId cid)
        {
            id = cid;
        }
    };

    template<typename T>
    EntityId ComponentTypeId<T>::id = 0;

    template<typename T>
    struct Store
    {
        T* store;
        uint32_t count;
        uint32_t capacity;

        Store() : 
            store(nullptr), count(0), capacity(0)
        {
        }

        void Init(WorldAllocator& wAllocator)
        {
            uint32_t storeCapacity = 8;
            count = 0;
            store =
                PTR_CAST(wAllocator.AllocN(sizeof(T), storeCapacity, storeCapacity), T);
            capacity = storeCapacity;
        }

        void Grow(WorldAllocator& wAllocator)
        {
            uint32_t newStoreCapacity = capacity * 2;
            T* newStore =
                PTR_CAST(wAllocator.AllocN(sizeof(T), newStoreCapacity, newStoreCapacity), T);

            std::memcpy(newStore, store, sizeof(T) * capacity);

            wAllocator.Free(sizeof(T) * capacity, store);

            store = newStore;
            capacity = newStoreCapacity;
        }

        template<typename U = T>
        void Add(U&& element)
        {
            store[count] = std::move(element);
            ++count;
        }

        void Destroy(WorldAllocator& wAllocator)
        {
            if(store)
            {
                wAllocator.Free(sizeof(T) * capacity, store);
            }
        }
    };

    struct ComponentSet
    {
        EntityId* idArr;
        uint32_t count;

        ComponentSet()
            : idArr(nullptr), count(0)
        {
        }

        ~ComponentSet() = default;

        ComponentSet(ComponentSet&& other) noexcept
        {
            idArr = other.idArr;
            count = other.count;

            other.idArr = nullptr;
            other.count = 0;            
        }

        ComponentSet& operator=(ComponentSet&& other) noexcept
        {
            idArr = other.idArr;
            count = other.count;

            other.idArr = nullptr;
            other.count = 0;

            return *this;
        }

        bool operator==(const ComponentSet& other)
        {
            if(count != other.count)
            {
                return false;
            }

            for(uint32_t i = 0; i < count; i++)
            {
                if(idArr[i] != other.idArr[i])
                {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(const ComponentSet& other)
        {
            if(count == other.count)
            {
                return true;
            }

            for(uint32_t i = 0; i < count; i++)
            {
                if(idArr[i] != other.idArr[i])
                {
                    return true;
                }
            }

            return false;
        }

        EntityId& operator[](uint32_t index)
        {
            if(index >= count)
            {
                assert(0);
            }

            return idArr[index];
        }

        uint64_t Hash() const
        {
            uint64_t h = 0;
            for(uint32_t i = 0; i < count; i++)
            {
                h += HashU64(idArr[i]);
            }

            h /= count;

            return h;
        }

        void Sort(){
            if(idArr)
            {
                std::sort(idArr, (idArr + count));
            }
        }

        int32_t Search(EntityId id)
        {
            EntityId* v = std::lower_bound(idArr, (idArr + count), id);
            
            if(v == (idArr + count) || *v != id)
            {
                return -1;
            }

            return static_cast<int32_t>(v - idArr);
        }

        int32_t SearchPair(EntityId id)
        {
            for(uint32_t idx = count ; idx > 0;)
            {
                --idx;
                if(LO_ENTITY_ID(id) == LO_ENTITY_ID(idArr[idx]))
                {
                    return idx;
                }
                
                if(HI_ENTITY_ID(idArr[idx]) == 0)
                {
                    return -1;
                }
            }

            return -1;
        }

        bool Has(EntityId id)
        {
            EntityId* v = std::lower_bound(idArr, (idArr + count), id);
            
            if(v == (idArr + count) || *v != id)
            {
                return false;
            }

            return true;
        }

        bool HasPair(EntityId id)
        {
            for(uint32_t idx = count ; idx > 0;)
            {
                --idx;
                if(LO_ENTITY_ID(id) == LO_ENTITY_ID(idArr[idx]))
                {
                    return true;
                }
                
                if(HI_ENTITY_ID(idArr[idx]) == 0)
                {
                    return false;
                }
            }

            return false;
        }

        void Init(WorldAllocator& wAllocator, uint32_t count)
        {
            idArr = PTR_CAST(wAllocator.Init(count * sizeof(EntityId)), EntityId);
            this->count = count;
        }

        void Destroy(WorldAllocator& wAllocator)
        {
            if(idArr)
            {
                wAllocator.Free(sizeof(EntityId) * count, idArr);
            }
        }

        void Clone(WorldAllocator& wAllocator, const ComponentSet& other)
        {
            count = other.count;
            Init(wAllocator, count);
            std::memcpy(idArr, other.idArr, count * sizeof(EntityId));
        }
    };

    template<>
    struct Hash<ComponentSet>
    {
        static uint64_t Value(const ComponentSet& v)
        {
            return v.Hash();
        }        
    };

    using CtorHook = void (*)(void* dest);
    using CopyCtorHook = void (*)(void* dest, const void* src);
    using MoveCtorHook = void (*)(void* dest, void* src);
    using DtorHook     = void (*)(void* src);

    using AddEventHook = void (*)();
    using RemoveEventHook = void (*)();
    using SetEventHook = void (*)(void* dest);

    struct TypeHook
    {
        void (*ctor)(void* dest);
        void (*copyCtor)(void* dest, const void* src);
        void (*moveCtor)(void* dest, void* src);
        void (*dtor)(void* src);

        void (*onAdd)();
        void (*onRemove)();
        void (*onSet)(void* dest);
    };

#define COMPONENT_TYPE      1 << 0
#define TAG_TYPE            1 << 1
#define PAIR_TYPE           1 << 2
#define TYPE_HAS_DATA       1 << 3
#define EXCLUSIVE_PAIR      1 << 4
#define BITSET_DATA         1 << 5
#define FULL_PAIR           1 << 6

    struct TypeInfo
    {
        EntityId id;
        uint32_t alignment;
        uint32_t size;
        TypeHook hook;
        uint32_t flags;

        bool HasData() const
        {
            return (flags & TYPE_HAS_DATA) == TYPE_HAS_DATA;
        }

        bool IsExclusive() const
        {
            return (flags & (EXCLUSIVE_PAIR | PAIR_TYPE)) ==  (EXCLUSIVE_PAIR | PAIR_TYPE);
        }

        bool IsDataBitset() const
        {
            return (flags & (TYPE_HAS_DATA | BITSET_DATA)) == (TYPE_HAS_DATA | BITSET_DATA);
        }

        bool IsFullPair() const
        {
            return (flags & (PAIR_TYPE | FULL_PAIR)) == (PAIR_TYPE | FULL_PAIR);
        }
    };

    struct Column
    {
        void* data;
        TypeInfo* typeInfo;
    };

    using ComponentDiff = ComponentSet;
    using ArchetypeId = uint32_t;

    constexpr uint32_t DefaultArchetypeCapacity = 4;

    struct Archetype
    {
        ArchetypeId id;
        uint32_t count;
        uint32_t capacity;
        uint32_t flags;
        Column* columns;
        EntityId* entities;
        ComponentSet componentSet;
        int32_t* componentMap;
        HashMap<EntityId, Archetype*> addEdges;
        HashMap<EntityId, Archetype*> removeEdges;
        uint32_t columnCount;

        Archetype()
            : id(0), count(0), capacity(0), flags(0),
            columns(nullptr), entities(nullptr), componentSet(), addEdges(), removeEdges()
        {
        }

        Archetype(Archetype&& other) noexcept
        {
            id = other.id;
            count = other.count;
            capacity = other.capacity;
            flags = other.flags;
            columnCount = other.columnCount;
            columns = other.columns;
            entities = other.entities;
            componentMap = other.componentMap;
            componentSet = std::move(other.componentSet);
            addEdges = std::move(other.addEdges);
            removeEdges = std::move(other.removeEdges);

            other.columns = nullptr;
            other.entities = nullptr;
            other.componentSet.idArr = nullptr;
            other.componentSet.count = 0;
        }

        Archetype& operator=(Archetype&& other) noexcept
        {
            id = other.id;
            count = other.count;
            capacity = other.capacity;
            flags = other.flags;
            columnCount = other.columnCount;
            columns = other.columns;
            entities = other.entities;
            componentMap = other.componentMap;
            componentSet = std::move(other.componentSet);
            addEdges = std::move(other.addEdges);
            removeEdges = std::move(other.removeEdges);

            other.columns = nullptr;
            other.entities = nullptr;
            other.componentSet.idArr = nullptr;
            other.componentSet.count = 0;

            return *this;
        }

    };

    inline ArchetypeId GetArchetypeId()
    {
        static ArchetypeId id = 0;

        return ++id;
    }

    struct ComponentRecord
    {
        EntityId id;
        Store<Archetype*> archetypeStore;
        TypeInfo* typeInfo;
#ifdef ECS_DEBUG
        char name[16];
#endif

    };

    struct EntityRecord
    {
        Archetype* archetype;
        uint32_t row;
        uint32_t dense;
    };


}

