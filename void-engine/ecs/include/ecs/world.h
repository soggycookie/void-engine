#pragma once
#include "ecs_pch.h"
#include "ecs_type.h"
#include "ds/world_allocator.h"
#include "type_info_builder.h"
#include "ds/hash_map.h"
#include "entity.h"
#include "system_meta.h"
#include "internal_component.h"
#include "entity_cmd.h"

namespace ECS
{
    struct Allocators
    {
        //BlockAllocator typeInfo;
        BlockAllocator archetypes;
    };

    class World
    {
    public:
        World()
            : m_nextFreeId(200), m_isDefered(false)
        {
        }

        void Init();

        void InitAllocators();

        void RegisterInternalComponents();

        Entity CreateEntity();
        Entity CreateEntity(EntityId parent);
        Entity CreateEntity(const char* name, EntityId parent);
        Entity CreateEntity(EntityId id, EntityId parent);
        Entity CreateEntity(EntityId id, const char* name, EntityId parent);

        Entity CreateEntity(EntityDesc& desc);

        EntityId GetNewId();
        EntityId GetReusedId();
        bool isEntityExist(EntityId eId);

        //the returned bool is true if this is a new id
        //else it is a reused one
        std::pair<bool, EntityId> GetId();

        EntityRecord* GetEntityRecord(EntityId eId);
        Entity GetEntity(EntityId eId);

        void ResolveEntityDesc(EntityRecord& r, EntityDesc& desc);

        template<typename T>
        TypeInfoBuilder<T> Component();

        template<typename T>
        TypeInfoBuilder<T> Tag();
        
        template<typename T>
        TypeInfoBuilder<T> Relationship(bool isExclusive, bool isToggle = false);

        template<typename T>
        void AddComponent(EntityId eId);

        template<typename Component>
        void RemoveComponent(EntityId eId);

        template<typename T>
        void AddRelationship(EntityId eId, EntityId targetId);

        template<typename T>
        void AddTag(EntityId eId);

        void AddComponent(EntityId eId, EntityId cId);

        void AddRelationship(EntityId eId, EntityId relationId, EntityId targetId);

        void AddTag(EntityId eId, EntityId cId);

        void RemoveComponent(EntityId eId, EntityId cId);

        template<typename T>
        void Set(EntityId eId, T&& c);

        template<typename T>
        T& Get(EntityId eId);

        void Set(EntityId eId, EntityId cId, void* data);

        void Set(EntityId eId, EntityId cId, const void* data);

        void* Get(EntityId eId, EntityId cId);

        void GrowArchetype(Archetype& archetype);

        void SwapBack(EntityRecord& r);

        Archetype* CreateArchetype(ComponentSet&& componentSet);
        Archetype* GetArchetype(const ComponentSet& componentSet);

        Archetype* GetOrCreateArchetype_Add(Archetype* src, EntityId cId);
        Archetype* GetOrCreateArchetype_Remove(Archetype* src, EntityId cId);

        void MoveArchetype_Add(EntityId eId, EntityRecord& r, Archetype* destArchetype);
        void MoveArchetype_Remove(EntityId eId, EntityRecord& r, Archetype* destArchetype);

        //Query


        //NOTE: System store list of cache archetypes, but the list can be invalidated at runtime,
        //so I need to find a new way to re-validate this or rewrite this in a different way
        //basically, I have to introduce sync point

        template<typename... Components, typename... FuncArgs>
        void System(void (*func)(FuncArgs...));

        template<typename... Components, typename... FuncArgs>
        void Each(void (*func)(FuncArgs...));

        void Progress(double dt);

        void Destroy();

    public:
        WorldAllocator m_wAllocator;
        Allocators m_allocators;
        SparseSet<EntityRecord> m_entityIndex;
        SparseSet<Archetype> m_archetypes;
        HashMap<EntityId, ComponentRecord> m_componentIndex;
        HashMap<EntityId, TypeInfo*> m_typeInfos;
        HashMap<ComponentSet, Archetype*> m_mappedArchetype; //value hold a ref to key, does not change the value's key ref
        Store<EntityId> m_componentStore;
        Store<SystemCallback> m_systemStore;
        uint32_t m_nextFreeId;
        bool m_isDefered;
    };
}

#include "type_info_builder.inl"
#include "world.inl"
#include "entity_cmd.inl"