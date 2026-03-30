#include "ecs_type.h"
#include "internal_component.h"
#include <cassert>
#include <string_view>
#include <type_traits>
#ifdef __clang__
#pragma once
#include "type_info.h"
#include "world.h"
#endif

namespace ECS
{
template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Ctor(CtorHook ctor)
{
    ti.hook.ctor = ctor;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::CopyCtor(CopyCtorHook cctor)
{
    ti.hook.copyCtor = cctor;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::MoveCtor(MoveCtorHook mctor)
{
    ti.hook.moveCtor = mctor;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Dtor(DtorHook dtor)
{
    ti.hook.dtor = dtor;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::AddEvent(AddEventHook e)
{
    ti.hook.onAdd = e;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::RemoveEvent(RemoveEventHook e)
{
    ti.hook.onRemove = e;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::SetEvent(SetEventHook e)
{
    ti.hook.onSet = e;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Id(EntityId id)
{
    ti.eId = id;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Singleton()
{
    if (!ti.IsComponent())
    {
        assert(0 && "Type must be a component to be a singleton!");
    }

    ti.flags |= SINGLETON_TYPE;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Exclusive()
{
    if (!ti.IsRelation())
    {
        assert(0);
    }

    ti.flags |= EXCLUSIVE_RELATION;

    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::HasData()
{
    ti.flags |= TYPE_HAS_DATA;

    return *this;
}

template <typename T>
void TypeInfoBuilder<T>::Register()
{
    assert(world);

    // Get Valid id here to assign type -> id
    if (world->IsEntityExist(ti.eId))
    {
        ti.eId = world->GetResuedOrNewId().second;
    }

    // void is for runtime relationship creation
    // that you only know type id of relation and target

    if constexpr (std::is_void_v<T>)
    {
        assert(first != 0);
        EcsName firstName = world->Get<EcsName>(first);
        if (!ti.IsRelationship())
        {
            assert(0);
        }
        else
        {
            // NOTE:
            assert(second != 0);
            EcsName targetName = world->Get<EcsName>(second);
            world->Register(ti, first, second,
                            std::string_view(firstName.name, 16),
                            std::string_view(targetName.name, 16));
        }
    }
    else
    {
        if (!ti.IsRelationship())
        {
            ComponentTypeId<T>::Id(ti.eId);
            world->Register(ti, first, second, GetComponentName<T>(),
                            std::string_view(nullptr, 0));
        }
        else
        {
            // NOTE:
            EcsName targetName = world->Get<EcsName>(second);
            world->Register(ti, first, second, GetComponentName<T>(),
                            std::string_view(targetName.name, 16));
        }
    }
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Component()
{
    ti.flags |= (COMPONENT_TYPE | TYPE_HAS_DATA);
    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Tag()
{
    ti.flags |= TAG_TYPE;
    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Relation()
{
    ti.flags |= RELATION_TYPE;
    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Relationship(EntityId relationId,
                                                     EntityId targetId)
{
    if constexpr (!std::is_void_v<T>)
    {
        static_assert(0);
    }
    assert(relationId);
    assert(targetId);
    if(!world->IsEntityExist(targetId) || !world->IsEntityExist(relationId))
    {
        assert(0);
    }

    if (!world->m_typeInfos.ContainsKey(relationId))
    {
        assert(0);
    }

    this->second = targetId;
    this->first = relationId;

    TypeInfo *relationTi = world->m_typeInfos[relationId];
    ti.size = relationTi->size;
    ti.alignment = relationTi->alignment;
    ti.hook = relationTi->hook;
    ti.flags |= RELATIONSHIP_TYPE;

    //if (world->m_typeInfos.ContainsKey(targetId))
    //{
    //    TypeInfo *targetTi = world->m_typeInfos[targetId];
    //    assert(!targetTi->IsRelation());
    //}

    assert(relationTi);
    assert(relationTi->IsRelation());


    return *this;
}

template <typename T>
TypeInfoBuilder<T> &TypeInfoBuilder<T>::Relationship(EntityId targetId)
{
    if constexpr (std::is_void_v<T>)
    {
        static_assert(0);
    }
    static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);

    assert(targetId);

    if(!world->IsEntityExist(targetId))
    {
        assert(0);
    }

    if (!world->m_typeInfos.ContainsKey(ComponentTypeId<T>::Id()))
    {
        assert(0);
    }

    this->first = ComponentTypeId<T>::Id();
    this->second = targetId;

    TypeInfo *relationTi = world->m_typeInfos[ComponentTypeId<T>::Id()];
    ti.size = relationTi->size;
    ti.alignment = relationTi->alignment;
    ti.hook = relationTi->hook;
    ti.flags |= RELATIONSHIP_TYPE;
    //if (world->m_typeInfos.ContainsKey(targetId))
    //{
    //    TypeInfo *targetTi = world->m_typeInfos[targetId];
    //    assert(!targetTi->IsRelation());
    //}

    assert(relationTi);
    assert(relationTi->IsRelation());


    return *this;
}

} // namespace ECS
