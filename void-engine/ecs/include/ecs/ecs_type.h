#pragma once
#include "ecs_pch.h"
#include "ds/hash_map.h"

namespace ECS
{
    using ComponentId = uint32_t;

    inline ComponentId GetNextComponentId()
    {
        static ComponentId id = 0;
        return id++;
    }

    template<typename T>
    ComponentId GetComponentId()
    {
        static ComponentId id = GetNextComponentId();
        return id;
    }

     

    struct ComponentSet
    {
        ComponentId* ids;
        uint32_t count;

        ComponentSet()
            : ids(nullptr), count(0)
        {
        }

        ~ComponentSet() = default;

        ComponentSet(ComponentSet&& other) noexcept
        {
            ids = other.ids;
            count = other.count;

            other.ids = nullptr;
            other.count = 0;            
        }

        ComponentSet(const ComponentSet& other)
        {
            ids = other.ids;
            count = other.count;
        }

        ComponentSet& operator=(ComponentSet&& other) noexcept
        {
            ids = other.ids;
            count = other.count;

            other.ids = nullptr;
            other.count = 0;

            return *this;
        }

        ComponentSet& operator=(const ComponentSet& other)
        {
            ids = other.ids;
            count = other.count;

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
                if(ids[i] != other.ids[i])
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
                if(ids[i] != other.ids[i])
                {
                    return true;
                }
            }

            return false;
        }

        uint64_t Hash() const
        {
            uint64_t h = 0;
            for(uint32_t i = 0; i < count; i++)
            {
                h += HashU64(ids[i]);
            }

            h /= count;

            return h;
        }

        void Sort(){
            std::sort(ids, (ids + count));
        }

        int32_t Search(ComponentId id)
        {
            ComponentId* v = std::lower_bound(ids, (ids + count), id);
            
            if(v == (ids + count) || *v != id)
            {
                return -1;
            }

            return static_cast<int32_t>(v - ids);
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

    struct TypeInfo
    {
        ComponentId id;
        uint32_t alignment;
        uint64_t size;
        EntityId entityId;
        const char* name;
        TypeHook hook;

    };

    template<typename Component>
    struct TypeInfoBuilder
    {
        TypeInfo& ti;

        void Ctor(CtorHook ctor)
        {
            ti.hook.ctor = ctor;
        }

        void CopyCtor(CopyCtorHook cctor)
        {
            ti.hook.copyCtor = cctor;
        }

        void MoveCtor(MoveCtorHook mctor)
        {
            ti.hook.moveCtor = mctor;
        }

        void Dtor(DtorHook dtor)
        {
            ti.hook.dtor = dtor;
        }

        void AddEvent(AddEventHook e)
        {
            ti.hook.onAdd = e;
        }
        
        void RemoveEvent(RemoveEventHook e)
        {
            ti.hook.onRemove = e;
        }
        
        void SetEvent(SetEventHook e)
        {
            ti.hook.onSet = e;
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
        ComponentSet components;
        HashMap<ComponentDiff, Archetype*> addedEdges;
        HashMap<ComponentDiff, Archetype*> removedEdges;

        Archetype()
            : id(0), count(0), capacity(0), flags(0),
            columns(nullptr), entities(nullptr), components(), addedEdges(), removedEdges()
        {
        }

        Archetype(Archetype&& other) noexcept
        {
            id = other.id;
            count = other.count;
            capacity = other.capacity;
            flags = other.flags;
            columns = other.columns;
            entities = other.entities;
            components = std::move(other.components);
            addedEdges = std::move(other.addedEdges);
            removedEdges = std::move(other.removedEdges);

            other.columns = nullptr;
            other.entities = nullptr;
            other.components.ids = nullptr;
            other.components.count = 0;
        }

        Archetype& operator=(Archetype&& other) noexcept
        {
            columns = other.columns;
            entities = other.entities;
            components = std::move(other.components);
            addedEdges = std::move(other.addedEdges);
            removedEdges = std::move(other.removedEdges);

            other.columns = nullptr;
            other.entities = nullptr;
            other.components.ids = nullptr;
            other.components.count = 0;

            return *this;
        }

    };

    inline ArchetypeId GetArchetypeId()
    {
        static ArchetypeId id = 0;

        return ++id;
    }

    struct ArchetypeCache
    {
        Archetype** archetypes;
        uint32_t count;
    };

    struct ComponentRecord
    {
        ComponentId id;
        ArchetypeCache archetypeCache;
        TypeInfo* typeInfo;
    };

    struct EntityRecord
    {
        Archetype* archetype;
        uint32_t row;
        uint32_t dense;
        char name[16];
    };

}

template<typename T>
struct ComponentName
{
    static constexpr const char* name;
};

#define ECS_COMPONENT(T) \
    template<> \
    struct ComponentName<T> \
    { \
        static constexpr const char* name = #T; \
    }; 