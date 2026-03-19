#pragma once
#include "ecs_type.h"
#include <type_traits>

namespace ECS
{
    class World;

    using CtorHook = void (*)(void* dest);
    using CopyCtorHook = void (*)(void* dest, const void* src);
    using MoveCtorHook = void (*)(void* dest, void* src);
    using DtorHook = void (*)(void* src);

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

#define COMPONENT_TYPE 1 << 0
#define TAG_TYPE 1 << 1
#define RELATION_TYPE 1 << 2
#define TYPE_HAS_DATA 1 << 3
#define EXCLUSIVE_RELATION 1 << 4
#define BITSET_DATA 1 << 5
#define RELATIONSHIP_TYPE 1 << 6
#define SINGLETON_TYPE 1 << 7

    struct TypeInfo
    {
        EntityId eId;
        EntityId cId;
        uint32_t alignment;
        uint32_t size;
        TypeHook hook;
        uint32_t flags;

        bool HasData() const { return (flags & TYPE_HAS_DATA) == TYPE_HAS_DATA; }

        bool IsExclusive() const
        {
            return (flags & (EXCLUSIVE_RELATION | RELATION_TYPE)) ==
                (EXCLUSIVE_RELATION | RELATION_TYPE);
        }

        bool IsDataBitset() const
        {
            return (flags & (TYPE_HAS_DATA | BITSET_DATA)) ==
                (TYPE_HAS_DATA | BITSET_DATA);
        }

        bool IsRelationship() const
        {
            return (flags & RELATIONSHIP_TYPE) == RELATIONSHIP_TYPE;
        }

        bool IsRelation() const { return (flags & RELATION_TYPE) == RELATION_TYPE; }

        bool IsComponent() const
        {
            return (flags & COMPONENT_TYPE) == COMPONENT_TYPE;
        }

        bool IsTag() const { return (flags & TAG_TYPE) == TAG_TYPE; }

        bool IsSingleton() const
        {
            return (flags & SINGLETON_TYPE) == SINGLETON_TYPE;
        }
    };

    template<typename T = void>
    struct TypeInfoBuilder
    {
        TypeInfo ti;
        World* world;
        EntityId first;
        EntityId second;

        TypeInfoBuilder(World* world)
            : ti(), world(world), second(0), first(0)
        {
            if constexpr(!std::is_void_v<T>)
            {
                ti.size = sizeof(T);
                ti.alignment = alignof(T);
            }

            ti.eId = 0;
        }

        TypeInfoBuilder<T>& Ctor(CtorHook ctor);

        TypeInfoBuilder<T>& CopyCtor(CopyCtorHook cctor);

        TypeInfoBuilder<T>& MoveCtor(MoveCtorHook mctor);

        TypeInfoBuilder<T>& Dtor(DtorHook dtor);

        TypeInfoBuilder<T>& AddEvent(AddEventHook e);

        TypeInfoBuilder<T>& RemoveEvent(RemoveEventHook e);

        TypeInfoBuilder<T>& SetEvent(SetEventHook e);

        TypeInfoBuilder<T>& Id(EntityId id);

        TypeInfoBuilder<T>& Singleton();

        void Register();

    private:
        friend class World;

        TypeInfoBuilder<T>& Exclusive();

        TypeInfoBuilder<T>& HasData();

        TypeInfoBuilder<T>& Component();

        TypeInfoBuilder<T>& Tag();

        TypeInfoBuilder<T>& Relation();

        TypeInfoBuilder<T>& Relationship(EntityId relationId, EntityId targetId);

        TypeInfoBuilder<T>& Relationship(EntityId targetId);
    };
}
