#include "ecs_type.h"
#ifdef __clang__
#pragma once
#include "type_info.h"
#include "world.h"
#endif

namespace VoidEngine::ECS
{
template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::ctor(CtorHook ctor)
{
    type_meta.hook.ctor = ctor;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::copy_ctor(CopyCtorHook cctor)
{
    non_trivial();
    type_meta.hook.copy_ctor = cctor;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::move_ctor(MoveCtorHook mctor)
{
    non_trivial();
    type_meta.hook.move_ctor = mctor;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::dtor(DtorHook dtor)
{
    non_trivial();
    type_meta.hook.dtor = dtor;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::add_event(AddEventHook e)
{
    type_meta.hook.on_add = e;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::remove_event(RemoveEventHook e)
{
    type_meta.hook.on_remove = e;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::set_event(SetEventHook e)
{
    ENGINE_ASSERT(!type_meta.is_tag(), "Tag cannot have set event!");
    type_meta.hook.on_set = e;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::id(EntityId id)
{
    type_meta.tid = id;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::singleton()
{
    ENGINE_ASSERT(type_meta.is_component(), "Singleton can only be component!");
    type_meta.flags |= SINGLETON_TYPE;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::exclusive()
{
    ENGINE_ASSERT(type_meta.is_relation(), "Only relation can be exclusive!");
    type_meta.flags |= EXCLUSIVE_TYPE;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::has_data()
{
    type_meta.flags |= DATA_TYPE;
    return *this;
}

template <typename T>
void TypeMetaBuilder<T>::register_type()
{
    world->register_type_metadata<T>(std::move(type_meta), get_type_name<T>());
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::component()
{
    type_meta.flags |= (COMPONENT_TYPE | DATA_TYPE | TRIVIAL_DATA);
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::tag()
{
    type_meta.flags |= (TAG_TYPE | TRIVIAL_DATA);
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::relation()
{
    type_meta.flags |= RELATION_TYPE;
    return *this;
}

template <typename T>
TypeMetaBuilder<T> &TypeMetaBuilder<T>::non_trivial()
{
    type_meta.flags &= ~TRIVIAL_DATA;
    return *this;
}

// template <typename T>
// TypeInfoBuilder<T> &TypeInfoBuilder<T>::Relationship(EntityId relationId,
//                                                      EntityId targetId)
// {
//     if constexpr (!std::is_void_v<T>)
//     {
//         static_assert(0);
//     }
//     assert(relationId);
//     assert(targetId);
//     if(!world->IsEntityExist(targetId) || !world->IsEntityExist(relationId))
//     {
//         assert(0);
//     }
//
//     if (!world->m_typeInfos.ContainsKey(relationId))
//     {
//         assert(0);
//     }
//
//     this->second = targetId;
//     this->first = relationId;
//
//     TypeInfo *relationTi = world->m_typeInfos[relationId];
//     ti.size = relationTi->size;
//     ti.alignment = relationTi->alignment;
//     ti.hook = relationTi->hook;
//     ti.flags |= RELATIONSHIP_TYPE;
//
//     //if (world->m_typeInfos.ContainsKey(targetId))
//     //{
//     //    TypeInfo *targetTi = world->m_typeInfos[targetId];
//     //    assert(!targetTi->IsRelation());
//     //}
//
//     assert(relationTi);
//     assert(relationTi->IsRelation());
//
//
//     return *this;
// }
//
// template <typename T>
// TypeInfoBuilder<T> &TypeInfoBuilder<T>::Relationship(EntityId targetId)
// {
//     if constexpr (std::is_void_v<T>)
//     {
//         static_assert(0);
//     }
//     static_assert(!std::is_reference_v<T> && !std::is_const_v<T>);
//
//     assert(targetId);
//
//     if(!world->IsEntityExist(targetId))
//     {
//         assert(0);
//     }
//
//     if (!world->m_typeInfos.ContainsKey(ComponentTypeId<T>::Id()))
//     {
//         assert(0);
//     }
//
//     this->first = ComponentTypeId<T>::Id();
//     this->second = targetId;
//
//     TypeInfo *relationTi = world->m_typeInfos[ComponentTypeId<T>::Id()];
//     ti.size = relationTi->size;
//     ti.alignment = relationTi->alignment;
//     ti.hook = relationTi->hook;
//     ti.flags |= RELATIONSHIP_TYPE;
//     //if (world->m_typeInfos.ContainsKey(targetId))
//     //{
//     //    TypeInfo *targetTi = world->m_typeInfos[targetId];
//     //    assert(!targetTi->IsRelation());
//     //}
//
//     assert(relationTi);
//     assert(relationTi->IsRelation());
//
//
//     return *this;
// }
//
} // namespace VoidEngine::ECS
