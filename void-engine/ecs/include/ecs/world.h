#pragma once
#include "ds/block_allocator.h"
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "entity.h"
#include "internal_component.h"
#include "query.h"
#include "type_info.h"

namespace ECS
{
struct Allocators
{
    // BlockAllocator typeInfo;
    BlockAllocator archetypes;
    BlockAllocator queries;
};

class World
{
public:
    World() : m_nextFreeId(200), m_isDefered(false), m_q_revalSweep_archetype(0)
    {
    }

    static constexpr const char *DefaultEntityName = "Entity %u";
    static constexpr const size_t MaxEntityNameLength = 32;

    void Bootstrap();

    void InitAllocators();

    void RegisterInternalComponents();
    void InitDefaultPipelinePhase();

    Entity CreateEntity(const char *name = nullptr, EntityId parent = 0);
    Entity CreateEntity(char *name, EntityId parent = 0);
    Entity CreateEntity(EntityId id, const char *name = nullptr,
                        EntityId parent = 0);

    void RemoveEntity(EntityId eId);

    EntityId GetNewId();
    EntityId GetReusedId();
    bool IsEntityExist(EntityId eId);
    bool IsEntityVersionOutdated(EntityId eId);

    // the returned bool is true if this is a new id
    // else it is a reused one
    std::pair<bool, EntityId> GetResuedOrNewId();

    EntityRecord *GetEntityRecord(EntityId eId);
    Entity GetEntity(EntityId eId);

    void ChildOf(EntityId eId, EntityId parentId);

    EntityId Parent(EntityId eId);

    Store<EntityId> GetChildren(EntityId eId);

    Entity ResolveEntityDesc(EntityDesc &desc);

    Archetype *GetEntityArchetype(EntityId eId);

    template <typename T>
    TypeInfoBuilder<T> Component();

    template <typename T>
    TypeInfoBuilder<T> Tag();

    template <typename T>
    TypeInfoBuilder<T> Relation();

    template <typename T>
    TypeInfoBuilder<T> Relationship(EntityId targetId);

    void Register(const TypeInfo &typeInfo, EntityId relationId,
                  EntityId targetId, const std::string_view first,
                  const std::string_view second);

    template <typename T>
    void AddComponent(EntityId eId);

    template <typename Component>
    void RemoveComponent(EntityId eId);

    template <typename T>
    void AddRelationship(EntityId eId, EntityId targetId);

    template <typename T>
    void AddTag(EntityId eId);

    template <typename T>
    bool HasComponent(EntityId eId);

    void AddComponent(EntityId eId, EntityId cId);

    void AddRelationship(EntityId eId, EntityId relationId, EntityId targetId);

    void AddTag(EntityId eId, EntityId cId);

    void RemoveComponent(EntityId eId, EntityId cId);

    bool HasComponent(EntityId eId, EntityId cId);

    bool HasRelationship(EntityId eId, EntityId first, EntityId second);

    bool HasRelationship(EntityId eId, EntityId cId);

    template <typename T>
    void Set(EntityId eId, T &&c);

    template <typename T>
    T &Get(EntityId eId);

    template <typename T>
    T &GetSingleton();

    void Set(EntityId eId, EntityId cId, void *data);

    void Set(EntityId eId, EntityId cId, const void *data);

    void *Get(EntityId eId, EntityId cId);

    void GrowArchetype(Archetype &archetype);

    void SwapBack(EntityRecord &r);

    Archetype *CreateArchetype(ComponentSet &&componentSet);
    Archetype *GetArchetype(const ComponentSet &componentSet);

    Archetype *GetOrCreateArchetype_Add(Archetype *src, EntityId cId);
    Archetype *GetOrCreateArchetype_Remove(Archetype *src, EntityId cId);

    void MoveArchetype_Add(EntityId eId, EntityRecord &r,
                           Archetype *destArchetype);
    void MoveArchetype_Remove(EntityId eId, EntityRecord &r,
                              Archetype *destArchetype);

    // Query

    template <typename... T>
    QueryBuilder<T...> CreateQuery();

    enum class EntityRevalidationMode
    {
        ON_ADDED,
        ON_REMOVED,
        ON_MODIFIED,
    };

    void RevalidateCachedQuery_EntityFilter(Archetype *archetype,
                                            uint32_t removeRow,
                                            EntityRevalidationMode mode);

    // if newArchetype = false -> archetype is removed
    void RevalidateCachedQuery_ArchetypeFilter(ComponentRecord &cr,
                                               Archetype *archetype,
                                               bool newArchetype);

    // NOTE: System store list of cache archetypes, but the list can be
    // invalidated at runtime, so I need to find a new way to re-validate this
    // or rewrite this in a different way basically, I have to introduce sync
    // point


    void Destroy();

public:
    WorldAllocator m_wAllocator;
    Allocators m_allocators;
    SparseSet<EntityRecord> m_entityIndex;
    SparseSet<Archetype> m_archetypes;
    HashMap<EntityId, ComponentRecord> m_componentIndex;
    HashMap<EntityId, TypeInfo *> m_typeInfos;
    HashMap<ComponentSet, Archetype *>
        m_mappedArchetype; // value hold a ref to key, does not change the
                           // value's key ref
    Store<EntityId> m_componentStore;
    //Store<SystemCallback> m_systemStore;
    uint32_t m_q_revalSweep_archetype;
    uint32_t m_nextFreeId;
    bool m_isDefered;
};
} // namespace ECS

#include "entity_cmd.inl"
#include "query.inl"
#include "type_info.inl"
#include "world.inl"
