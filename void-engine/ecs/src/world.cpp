#include "world.h"
#include "ecs_type.h"
#include "ecs_utils.h"
#include "entity.h"
#include "internal_component.h"
#include "query.h"
#include <type_traits>

namespace ECS
{
World *CreateWorld()
{
    auto w = new World();
    w->Bootstrap();
    // w->CreateInternalEntity();
    // w->RegisterInternalComponents();
    return w;
}

void DestroyWorld(World *world)
{
    world->Destroy();
    delete world;
}

void World::Bootstrap()
{
    m_wAllocator.Init();
    InitAllocators();
    m_entityIndex.Init(&m_wAllocator, nullptr, 8, true);
    m_archetypes.Init(&m_wAllocator, &m_allocators.archetypes, 8, false);
    m_componentIndex.Init(&m_wAllocator, 8);
    m_typeInfos.Init(&m_wAllocator, 8);
    m_mappedArchetype.Init(&m_wAllocator, 8);

    //m_systemStore.Init(m_wAllocator);
    m_componentStore.Init(m_wAllocator);
    m_isDefered = false;

    RegisterInternalComponents();
    InitDefaultPipelinePhase();
}

void World::InitAllocators()
{
    m_allocators.archetypes.Init(SparsePageCount * sizeof(Archetype));
    m_allocators.queries.Init(SparsePageCount * sizeof(Query));
}

void World::RegisterInternalComponents()
{
    Component<EcsName>().Id(EcsNameId).Register();
    Component<EcsInherit>().Id(EcsInheritId).Register();
    Component<EcsSystem>().Id(EcsSystemId).Register();
    Component<EcsQuery>().Id(EcsQueryId).Register();
    Component<EcsInputManager>().Id(EcsInputManagerId).Singleton().Register();
    Component<EcsTime>().Id(EcsTimeId).Singleton().Register();

    Tag<EcsPhase>().Id(EcsPhaseId).Register();
    Tag<EcsArchetype>().Id(EcsArchetypeId).Register();
    Tag<EcsPipeline>().Id(EcsPipelineId).Register();
    Tag<EcsDisabled>().Id(EcsDisabledId).Register();

    Relation<EcsChildOf>().Id(EcsChildOfId).Exclusive().Register();
    Relation<EcsDependOn>().Id(EcsDependOnId).Register();
    Relation<EcsToggle>().Id(EcsToggleId).Register();
    Relation<EcsIsA>().Id(EcsIsAId).Register();
}

void World::InitDefaultPipelinePhase()
{
    CreateEntity(EcsOnBootId, EcsOnBoot, EcsInvalidId).AddTag<EcsPhase>();
    CreateEntity(EcsOnStartId, EcsOnStart, EcsOnBootId).AddTag<EcsPhase>();

    CreateEntity(EcsOnLoopId, EcsOnLoop, EcsInvalidId).AddTag<EcsPhase>();
    CreateEntity(EcsOnValidationId, EcsOnValidation, EcsOnLoopId)
        .AddTag<EcsPhase>();
    CreateEntity(EcsOnStartFrameId, EcsOnStartFrame, EcsOnValidationId)
        .AddTag<EcsPhase>();
    CreateEntity(EcsOnPreUpdateId, EcsOnPreUpdate, EcsOnStartFrameId)
        .AddTag<EcsPhase>();
    CreateEntity(EcsOnUpdateId, EcsOnUpdate, EcsOnPreUpdateId)
        .AddTag<EcsPhase>();
    CreateEntity(EcsOnPostUpdateId, EcsOnPostUpdate, EcsOnUpdateId)
        .AddTag<EcsPhase>();
    CreateEntity(EcsOnEndFrameId, EcsOnEndFrame, EcsOnPostUpdateId)
        .AddTag<EcsPhase>();
}

// Register Type
// World do this because of letting TypeInfoBuilder do it seem messy
// return eId to make TypeInfoBuilder map Type -> eId (not work on relationship)
void World::Register(const TypeInfo &typeInfo, EntityId relationId,
                     EntityId targetId, const std::string_view first,
                     const std::string_view second)
{
    TypeInfo *ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
    std::memcpy(ti, &typeInfo, sizeof(TypeInfo));

    size_t firstSize = first.size();
    size_t secondSize = second.size();

    // reserve last char for null terminator
    if (firstSize >= MaxEntityNameLength)
    {
        firstSize = MaxEntityNameLength - 1;
        secondSize = 0;
    }
    else
    {
        // minus one for the space between 2 names
        size_t maxSecondSize = (MaxEntityNameLength - 1) - firstSize - 1;

        if (secondSize > maxSecondSize)
        {
            secondSize = maxSecondSize;
        }
    }

    char name[MaxEntityNameLength];
    std::memcpy(name, first.data(), firstSize);

    if (secondSize != 0)
    {
        name[firstSize] = ' ';
        std::memcpy(name + firstSize + 1, second.data(), secondSize);
        name[firstSize + 1 + secondSize] = '\0';
    }
    else
    {
        name[firstSize] = '\0';
    }

    if (m_componentStore.capacity == m_componentStore.count)
    {
        m_componentStore.Grow(m_wAllocator);
    }

    if (ti->IsRelationship())
    {
        ti->cId = MakeRelationship(relationId, targetId);
    }
    else
    {
        ti->cId = ti->eId;
        m_componentStore.Add(m_wAllocator, ti->cId);
    }

    ComponentRecord cr;
    cr.id = ti->eId;
    cr.typeInfo = ti;
    cr.archetypeStore.Init(m_wAllocator);

#ifdef ECS_DEBUG
    std::memcpy(cr.name, name, strlen(name));
#endif

    assert(cr.archetypeStore.store);

    m_componentIndex.Insert(ti->cId, std::move(cr));
    m_typeInfos.Insert(ti->cId, ti);

    EntityDesc eDesc(ti->eId, 0, name);

    if (ti->IsSingleton())
    {
        eDesc.Add(m_wAllocator, ti->cId, nullptr);
    }

    ResolveEntityDesc(eDesc);
}

Entity World::CreateEntity(const char *name, EntityId parent)
{
    return CreateEntity(0, name, parent);
}

Entity World::CreateEntity(char *name, EntityId parent)
{
    return CreateEntity(0, name, parent);
}

Entity World::CreateEntity(EntityId id, const char *name, EntityId parent)
{

    EntityDesc desc;
    desc.eId = id;
    desc.name = name;
    desc.parentId = parent;

    return ResolveEntityDesc(desc);
}

bool World::IsEntityExist(EntityId eId)
{
    return m_entityIndex.IsExisting(eId);
}

bool World::IsEntityVersionOutdated(EntityId eId)
{
    if (eId == EcsInvalidId)
    {
        return false;
    }

    if (!IsEntityExist(eId))
    {
        assert(0);
    }

    EntityId matchedId = m_entityIndex.GetId(m_entityIndex.GetDenseIndex(eId));

    return ENTITY_GEN_COUNT(matchedId) != ENTITY_GEN_COUNT(eId);
}

EntityId World::GetNewId()
{
    while (m_entityIndex.IsExisting(++m_nextFreeId))
        ;

    return m_nextFreeId;
}

EntityId World::GetReusedId()
{
    EntityId reusedId = m_entityIndex.GetReusedId();
    if (reusedId == EcsInvalidId)
    {
        return 0;
    }

    return INCRE_GEN_COUNT(reusedId);
}

std::pair<bool, EntityId> World::GetResuedOrNewId()
{
    bool newId = false;

    EntityId id = GetReusedId();

    if (id == EcsInvalidId)
    {
        newId = true;

        id = GetNewId();
    }

    return {newId, id};
}

Entity World::GetEntity(EntityId eId)
{
    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);
    uint32_t dense = r->dense;
    uint64_t versionedId = m_entityIndex.GetDenseArr()[dense];
    if (!r || versionedId != eId)
    {
        return Entity(0, this);
    }

    return Entity(eId, this);
}

EntityRecord *World::GetEntityRecord(EntityId eId)
{
    EntityRecord *e = m_entityIndex.GetPageData(eId);

    return e;
}

void World::ChildOf(EntityId eId, EntityId parentId)
{
    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    if (r->archetype->componentSet.HasRelationship(
            MakeRelationship(EcsChildOfId, EcsAnyId)))
    {
        assert(0);
    }

    AddRelationship(eId, EcsChildOfId, parentId);
}

EntityId World::Parent(EntityId eId)
{
    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    int32_t cIdx = r->archetype->componentSet.SearchRelationship(
        MakeRelationship(EcsChildOfId, EcsAnyId));

    if (cIdx == ComponentSet::NotFoundIdx)
    {
        return EcsInvalidId;
    }

    return HI_ENTITY_ID(r->archetype->componentSet[cIdx]);
}

Store<EntityId> World::GetChildren(EntityId eId)
{
    EntityId relationshipId = MakeRelationship(EcsChildOfId, eId);
    if (!m_componentIndex.ContainsKey(relationshipId))
    {
        return Store<EntityId>();
    }

    ComponentRecord &cr = m_componentIndex[relationshipId];
    Store<EntityId> store;

    for (size_t idx = 0; idx < cr.archetypeStore.count; ++idx)
    {
        for (size_t eIdx = 0; eIdx < cr.archetypeStore[idx]->count; ++eIdx)
        {
            store.Add(m_wAllocator, cr.archetypeStore[idx]->entities[eIdx]);
        }
    }

    return store;
}

Entity World::ResolveEntityDesc(EntityDesc &desc)
{
    bool newId = false;
    if (!m_entityIndex.IsExisting(desc.eId))
    {
        newId = true;
    }
    else
    {
        auto pair = GetResuedOrNewId();
        desc.eId = pair.second;
        newId = pair.first;
    }

    uint32_t dense = m_entityIndex.PushBack(desc.eId, EntityRecord{}, newId);
    EntityRecord &r = *m_entityIndex.GetPageData(desc.eId);
    r.dense = dense;

    char entityName[EcsNameLength];
    if (!desc.name)
    {
        std::snprintf(entityName, MaxEntityNameLength, DefaultEntityName,
                      LO_ENTITY_ID(desc.eId));
    }
    else
    {
        size_t len = std::strlen(desc.name);
        if (len >= MaxEntityNameLength)
        {
            len = MaxEntityNameLength - 1;
            // NOTE:
            // WANRING
        }

        std::memcpy(entityName, desc.name, len);
        entityName[len] = '\0';
    }

    Archetype *destArchetype = r.archetype;
    if (desc.parentId != EcsInvalidId)
    {
        if (!m_componentIndex.ContainsKey(MakeRelationship(
                ComponentTypeId<EcsChildOf>::Id(), desc.parentId)))
        {
            TypeInfoBuilder<EcsChildOf> tiBuilder(this);
            tiBuilder.Relationship(desc.parentId).Register();
        }

        destArchetype = GetOrCreateArchetype_Add(
            destArchetype,
            MakeRelationship(ComponentTypeId<EcsChildOf>::Id(), desc.parentId));
    }

    // if(desc.id != ComponentTypeId<EcsName>::Id())
    //{
    destArchetype =
        GetOrCreateArchetype_Add(destArchetype, ComponentTypeId<EcsName>::Id());
    //}

    for (uint32_t idx = 0; idx < desc.bulkComponents.count; ++idx)
    {
        destArchetype = GetOrCreateArchetype_Add(destArchetype,
                                                 desc.bulkComponents[idx].cId);
    }

    //////////// sort the cId //////////////
    desc.Sort();

    // TODO: Rewrite this
    size_t dataIncre = 0;
    if (destArchetype)
    {
        if (destArchetype->count == destArchetype->capacity)
        {
            GrowArchetype(*destArchetype);
        }

        for (size_t idx = 0; idx < destArchetype->componentSet.count; idx++)
        {
            // skip no data tag and pair
            int32_t destColIdx = destArchetype->componentMap[idx];

            if (destColIdx == -1)
            {
                continue;
            }

            Column &destCol = destArchetype->columns[destColIdx];
            TypeInfo &ti = *destCol.typeInfo;

            void *dest = OFFSET(destCol.data, ti.size * destArchetype->count);

            if (destArchetype->componentSet[idx] ==
                ComponentTypeId<EcsName>::Id())
            {
                EcsName ecsName;
                std::memcpy(ecsName.name, entityName, EcsNameLength);
                ti.hook.moveCtor(dest, &ecsName);
            }
            else
            {
                void *data = desc.bulkComponents[dataIncre++].data;

                if (data)
                {

                    if (ti.hook.moveCtor)
                    {
                        ti.hook.moveCtor(dest, data);
                    }
                    else if (ti.hook.copyCtor)
                    {
                        ti.hook.copyCtor(dest, data);
                    }
                    else
                    {
                        std::memcpy(dest, data, ti.size);
                    }
                }
                else
                {
                    if (ti.hook.ctor)
                    {
                        ti.hook.ctor(dest);
                    }
                }
            }
        }

        destArchetype->entities[destArchetype->count] = desc.eId;
        r.archetype = destArchetype;
        r.row = destArchetype->count;
        ++destArchetype->count;
    }

    desc.bulkComponents.Destroy(m_wAllocator);

    return Entity(desc.eId, this);
}

Archetype *World::GetEntityArchetype(EntityId eId)
{
    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    return r->archetype;
}

void World::RemoveEntity(EntityId eId)
{
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    SwapBack(*r);

    for (uint32_t idx = 0; idx < r->archetype->columnCount; idx++)
    {
        Column &col = r->archetype->columns[idx];
        TypeInfo *ti = col.typeInfo;

        assert(ti);

        if (ti->hook.dtor)
        {
            void *src =
                OFFSET_ELEMENT(col.data, ti->size, r->archetype->count - 1);

            ti->hook.dtor(src);
        }
    }

    --r->archetype->count;
    m_entityIndex.Remove(eId);
}

void World::AddComponent(EntityId eId, EntityId cId)
{
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[cId];

    assert(ti->IsComponent());
    assert(!ti->IsSingleton());
    assert(r);
    assert(!(r->archetype->componentSet.Has(cId)));

    Archetype *destArchetype = GetOrCreateArchetype_Add(r->archetype, cId);

    MoveArchetype_Add(eId, *r, destArchetype);

    ti->hook.onAdd();
}

void World::AddRelationship(EntityId eId, EntityId relationId,
                            EntityId targetId)
{
    if (IsEntityVersionOutdated(eId) || IsEntityVersionOutdated(targetId))
    {
        assert(0);
    }

    EntityId relationshipId = MakeRelationship(relationId, targetId);
    TypeInfo *ti = m_typeInfos.GetValue(relationId);

    assert(ti);
    assert(ti->IsRelation());
    assert(!ti->IsSingleton());

    if (!m_typeInfos.ContainsKey(relationshipId))
    {
        TypeInfoBuilder tiBuilder(this);
        tiBuilder.Relationship(relationId, targetId).Register();
    }

    TypeInfo *relationshipTi = m_typeInfos[relationshipId];

    EntityRecord *r = m_entityIndex.GetPageData(eId);

    assert(r);
    assert(!r->archetype->componentSet.Has(relationshipId));

    if (ti->IsExclusive())
    {
        if (r->archetype->componentSet.HasRelationship(relationId))
        {
            return;
        }
    }

    Archetype *destArchetype =
        GetOrCreateArchetype_Add(r->archetype, relationshipId);

    MoveArchetype_Add(eId, *r, destArchetype);

    relationshipTi->hook.onAdd();
}

void World::AddTag(EntityId eId, EntityId cId)
{
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[cId];

    assert(ti->IsTag());
    assert(r);
    assert(!r->archetype->componentSet.Has(cId));
    assert(!ti->IsSingleton());

    Archetype *destArchetype = GetOrCreateArchetype_Add(r->archetype, cId);

    MoveArchetype_Add(eId, *r, destArchetype);

    ti->hook.onAdd();
}

void World::RemoveComponent(EntityId eId, EntityId cId)
{
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);

    assert(r);
    assert(r->archetype);
    Archetype *srcArchetype = r->archetype;
    assert(r->archetype->componentSet.Has(cId));

    Archetype *destArchetype = GetOrCreateArchetype_Remove(srcArchetype, cId);

    MoveArchetype_Remove(eId, *r, destArchetype);

    TypeInfo *ti = m_typeInfos[cId];
    assert(ti);
    ti->hook.onRemove();
}

bool World::HasComponent(EntityId eId, EntityId cId)
{
    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    return r->archetype->componentSet.Has(cId);
}

bool World::HasRelationship(EntityId eId, EntityId first, EntityId second)
{
    return HasRelationship(eId, MakeRelationship(first, second));
}

bool World::HasRelationship(EntityId eId, EntityId cId)
{

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    return r->archetype->componentSet.HasRelationship(cId);
}
void World::Set(EntityId eId, EntityId cId, void *data)
{
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);
    assert(r->archetype);
    assert(r->dense);

    int32_t idx = r->archetype->componentSet.Search(cId);
    assert(idx != -1);

    int32_t colIdx = r->archetype->componentMap[idx];
    assert(colIdx != -1);

    Column &col = r->archetype->columns[colIdx];
    TypeInfo *ti = col.typeInfo;

    assert(ti);

    void *dest = OFFSET_ELEMENT(col.data, ti->size, r->row);

    if (ti->hook.moveCtor)
    {
        ti->hook.moveCtor(dest, data);
    }
    else if (ti->hook.copyCtor)
    {
        ti->hook.copyCtor(dest, data);
    }
    else
    {
        std::memcpy(dest, data, ti->size);
    }

    RevalidateCachedQuery_EntityFilter(r->archetype, r->row,
                                       EntityRevalidationMode::ON_MODIFIED);

    ti->hook.onSet(dest);
}

void World::Set(EntityId eId, EntityId cId, const void *data)
{
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);
    assert(r->archetype);
    assert(r->dense);

    int32_t idx = r->archetype->componentSet.Search(cId);
    assert(idx != -1);

    int32_t colIdx = r->archetype->componentMap[idx];
    assert(colIdx != -1);

    Column &col = r->archetype->columns[colIdx];
    TypeInfo *ti = col.typeInfo;

    assert(ti);

    void *dest = OFFSET_ELEMENT(col.data, ti->size, r->row);

    if (ti->hook.copyCtor)
    {
        ti->hook.copyCtor(dest, data);
    }
    else
    {
        std::memcpy(dest, data, ti->size);
    }

    RevalidateCachedQuery_EntityFilter(r->archetype, r->row,
                                       EntityRevalidationMode::ON_MODIFIED);

    ti->hook.onSet(dest);
}

void *World::Get(EntityId eId, EntityId cId)
{
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);
    assert(r->archetype);
    assert(r->dense);

    int32_t idx = r->archetype->componentSet.Search(cId);
    assert(idx != -1);
    int32_t colIdx = r->archetype->componentMap[idx];
    assert(colIdx != -1);

    Column &col = r->archetype->columns[colIdx];
    TypeInfo &ti = *col.typeInfo;

    void *component = OFFSET_ELEMENT(col.data, ti.size, r->row);

    return component;
}

void World::GrowArchetype(Archetype &archetype)
{
    uint32_t newCapacity = archetype.capacity * 2;

    // grow entities
    EntityId *newEntities =
        PTR_CAST(m_wAllocator.Init(sizeof(EntityId) * newCapacity), EntityId);

    std::memcpy(newEntities, archetype.entities,
                sizeof(EntityId) * archetype.capacity);
    m_wAllocator.Free(sizeof(EntityId) * archetype.capacity,
                      archetype.entities);

    archetype.entities = newEntities;

    // grow column data
    for (uint32_t idx = 0; idx < archetype.columnCount; idx++)
    {
        Column &col = archetype.columns[idx];
        TypeInfo *ti = col.typeInfo;
        void *newColData = m_wAllocator.Init(ti->size * newCapacity);

        if (ti->hook.moveCtor || ti->hook.copyCtor)
        {
            for (uint32_t row = 0; row < archetype.count; row++)
            {
                void *dest = OFFSET(newColData, ti->size * row);
                void *src = OFFSET(col.data, ti->size * row);

                if (ti->hook.moveCtor)
                {
                    ti->hook.moveCtor(dest, src);
                }
                else
                {
                    ti->hook.copyCtor(dest, src);
                }

                if (ti->hook.dtor)
                {
                    ti->hook.dtor(src);
                }
            }
        }
        else
        {
            std::memcpy(newColData, col.data, ti->size * archetype.count);
        }

        m_wAllocator.Free(ti->size * archetype.capacity, col.data);

        col.data = newColData;
    }

    archetype.capacity = newCapacity;
}

void World::SwapBack(EntityRecord &r)
{
    assert(r.archetype);
    assert(r.dense);

    Archetype &archetype = *r.archetype;

    if (archetype.count == 0 || (archetype.count == r.row + 1))
    {
        return;
    }

    EntityId swapId = archetype.entities[r.row];
    EntityId backId = archetype.entities[archetype.count - 1];

    EntityRecord *backRecord = m_entityIndex.GetPageData(backId);
    assert(backRecord);

    archetype.entities[r.row] = backId;
    archetype.entities[archetype.count - 1] = swapId;

    for (uint32_t i = 0; i < r.archetype->componentSet.count; i++)
    {
        Column &col = r.archetype->columns[i];
        TypeInfo &ti = *col.typeInfo;
        void *swap = OFFSET(col.data, ti.size * r.row);
        void *back = OFFSET(col.data, ti.size * (archetype.count - 1));

        void *temp = m_wAllocator.Init(ti.size);

        if (ti.hook.moveCtor)
        {
            ti.hook.moveCtor(temp, swap);
            ti.hook.moveCtor(swap, back);
            ti.hook.moveCtor(back, temp);
        }
        else if (ti.hook.copyCtor)
        {
            ti.hook.copyCtor(temp, swap);
            ti.hook.copyCtor(swap, back);
            ti.hook.copyCtor(back, temp);
        }
        else
        {
            std::memcpy(temp, swap, ti.size);
            std::memcpy(swap, back, ti.size);
            std::memcpy(back, temp, ti.size);
        }

        if (ti.hook.dtor)
        {
            ti.hook.dtor(temp);
        }

        m_wAllocator.Free(ti.size, temp);
    }

    backRecord->row = r.row;
    r.row = archetype.count - 1;
}

Archetype *World::CreateArchetype(ComponentSet &&componentSet)
{
    ComponentSet cloneSet;
    cloneSet.Clone(m_wAllocator, componentSet);

    ArchetypeId id = GetArchetypeId();
    Archetype archetype;
    archetype.id = id;
    archetype.count = 0;
    archetype.capacity = DefaultArchetypeCapacity;
    archetype.componentSet = std::move(cloneSet);
    archetype.addEdges.Init(&m_wAllocator, DefaultArchetypeCapacity);
    archetype.removeEdges.Init(&m_wAllocator, DefaultArchetypeCapacity);

    archetype.columns = PTR_CAST(
        m_wAllocator.Init(sizeof(Column) * componentSet.count), Column);
    archetype.entities =
        PTR_CAST(m_wAllocator.Init(sizeof(EntityId) * DefaultArchetypeCapacity),
                 EntityId);
    archetype.componentMap = PTR_CAST(
        m_wAllocator.Calloc(sizeof(int32_t) * componentSet.count * 2), int32_t);
    archetype.trackedQueries.Init(m_wAllocator, 4);

    // Set up component map to determine what column has data
    uint32_t dataColCounter = 0;
    for (uint32_t idx = 0; idx < componentSet.count; idx++)
    {
        TypeInfo *ti = m_typeInfos[LO_ENTITY_ID(componentSet[idx])];
        if (ti->HasData())
        {
            archetype.columns[dataColCounter].typeInfo = ti;
            archetype.columns[dataColCounter].data =
                m_wAllocator.Init(ti->size * DefaultArchetypeCapacity);

            archetype.componentMap[idx] = dataColCounter;
            archetype.componentMap[componentSet.count + dataColCounter] = idx;
            ++dataColCounter;
        }
        else
        {
            archetype.componentMap[idx] = -1;
        }
    }
    archetype.columnCount = dataColCounter;

    m_archetypes.PushBack(id, std::move(archetype));
    Archetype *rArchetype = m_archetypes.GetPageData(id);

    // Add newly created archetype to component record for query purpose
    // if component record is relationship,
    // add this archetype to base relation and specific relationship

    // Notify all the cached queries

    ++m_q_revalSweep_archetype;

    for (uint32_t idx = 0; idx < componentSet.count; idx++)
    {
        ComponentRecord &cr = m_componentIndex[componentSet[idx]];

        RevalidateCachedQuery_ArchetypeFilter(cr, rArchetype, true);

        // union pair
        if (cr.typeInfo->IsRelationship())
        {
            ComponentRecord &pCr =
                m_componentIndex[LO_ENTITY_ID(componentSet[idx])];

            RevalidateCachedQuery_ArchetypeFilter(pCr, rArchetype, true);

            if (pCr.archetypeStore.count == pCr.archetypeStore.capacity)
            {
                pCr.archetypeStore.Grow(m_wAllocator);
            }

            pCr.archetypeStore.store[pCr.archetypeStore.count] = rArchetype;
            ++pCr.archetypeStore.count;
        }

        if (cr.archetypeStore.count == cr.archetypeStore.capacity)
        {
            cr.archetypeStore.Grow(m_wAllocator);
        }

        cr.archetypeStore.store[cr.archetypeStore.count] = rArchetype;
        ++cr.archetypeStore.count;
    }

    m_mappedArchetype.Insert(std::move(componentSet), rArchetype);

    return rArchetype;
}

Archetype *World::GetArchetype(const ComponentSet &componentSet)
{
    if (m_mappedArchetype.ContainsKey(componentSet))
    {
        return m_mappedArchetype[componentSet];
    }

    return nullptr;
}

Archetype *World::GetOrCreateArchetype_Add(Archetype *src, EntityId cId)
{
    uint32_t srcCount = 0;
    if (src)
    {
        srcCount = src->componentSet.count;
    }

    Archetype *dest = nullptr;

    // find or create edge
    if (src)
    {
        if (src->addEdges.ContainsKey(cId))
        {
            dest = src->addEdges[cId];
        }
        else
        {
            // CREATE EDEGE
            uint32_t count = srcCount + 1;

            ComponentSet cs;
            cs.Init(m_wAllocator, count);
            std::memcpy(cs.idArr, src->componentSet.idArr,
                        srcCount * sizeof(EntityId));
            cs[count - 1] = cId;
            cs.Sort();

            dest = GetArchetype(cs);

            if (!dest)
            {
                dest = CreateArchetype(std::move(cs));
            }
            else
            {
                cs.Destroy(m_wAllocator);
            }

            src->addEdges.Insert(cId, dest);
        }
    }
    else
    {
        ComponentSet cs;
        cs.Init(m_wAllocator, 1);
        cs[0] = cId;

        dest = GetArchetype(cs);

        if (!dest)
        {
            dest = CreateArchetype(std::move(cs));
        }
        else
        {
            cs.Destroy(m_wAllocator);
        }
    }

    assert(dest);

    return dest;
}

Archetype *World::GetOrCreateArchetype_Remove(Archetype *src, EntityId cId)
{
    assert(src);

    uint32_t srcCount = 0;
    srcCount = src->componentSet.count;

    Archetype *dest = nullptr;

    if (src->removeEdges.ContainsKey(cId))
    {
        dest = src->removeEdges[cId];
    }
    else
    {
        // CREATE EDEGE
        uint32_t count = srcCount - 1;

        assert(count >= 0);
        if (count == 0)
        {
            return nullptr;
        }
        else
        {
            ComponentSet cs;
            cs.Init(m_wAllocator, count);

            // TODO: Review this later
            int32_t rIdx = src->componentSet.Search(cId);
            assert(rIdx != -1);

            std::memcpy(cs.idArr, src->componentSet.idArr,
                        rIdx * sizeof(EntityId));
            std::memcpy(cs.idArr + rIdx, src->componentSet.idArr + rIdx + 1,
                        (count - rIdx - 1) * sizeof(EntityId));
            cs.Sort();

            dest = GetArchetype(cs);

            if (!dest)
            {
                dest = CreateArchetype(std::move(cs));
            }
            else
            {
                m_wAllocator.Free(sizeof(EntityId) * cs.count, cs.idArr);
            }

            src->removeEdges.Insert(cId, dest);
        }
    }

    return dest;
}

void World::MoveArchetype_Add(EntityId eId, EntityRecord &r,
                              Archetype *destArchetype)
{
    Archetype *srcArchetype = r.archetype;
    assert(destArchetype);

    if (destArchetype->count == destArchetype->capacity)
    {
        GrowArchetype(*destArchetype);
    }

    // empty entity
    if (!srcArchetype)
    {
        if (destArchetype->columnCount == 1)
        {
            TypeInfo &ti = *destArchetype->columns[0].typeInfo;
            void *dataAddr = OFFSET(destArchetype->columns[0].data,
                                    destArchetype->count * ti.size);
            if (ti.hook.ctor)
            {
                ti.hook.ctor(dataAddr);
            }
            else
            {
                assert(0);
            }
        }
    }
    // at least 1 component
    else
    {
        // SWAP BACK IN SRC ARCHETYPE
        SwapBack(r);

        for (size_t idx = 0; idx < destArchetype->componentSet.count; idx++)
        {
            // skip no data tag and relationship
            int32_t destColIdx = destArchetype->componentMap[idx];

            if (destColIdx == -1)
            {
                continue;
            }

            Column &destCol = destArchetype->columns[destColIdx];
            TypeInfo &ti = *destCol.typeInfo;

            void *dest = OFFSET(destCol.data, ti.size * destArchetype->count);

            int32_t srcIndex = srcArchetype->componentSet.Search(
                destArchetype->componentSet[idx]);

            if (srcIndex == -1)
            {
                // find the newly added component
                ti.hook.ctor(dest);
            }
            else
            {
                int32_t srcColIdx = srcArchetype->componentMap[srcIndex];

                if (srcColIdx == -1)
                {
                    assert(0 && "Mismatch type");
                }

                Column &srcCol = srcArchetype->columns[srcColIdx];
                void *src = OFFSET(srcCol.data, ti.size * r.row);

                if (ti.hook.moveCtor)
                {
                    ti.hook.moveCtor(dest, src);
                }
                else if (ti.hook.copyCtor)
                {
                    ti.hook.copyCtor(dest, src);
                }
                else
                {
                    std::memcpy(dest, src, ti.size);
                }

                if (ti.hook.dtor)
                {
                    ti.hook.dtor(src);
                }
            }
        }

        --srcArchetype->count;
    }

    uint32_t removeRow = r.row;

    destArchetype->entities[destArchetype->count] = eId;
    r.archetype = destArchetype;
    r.row = destArchetype->count;
    ++destArchetype->count;

    RevalidateCachedQuery_EntityFilter(srcArchetype, removeRow,
                                       EntityRevalidationMode::ON_REMOVED);
    RevalidateCachedQuery_EntityFilter(destArchetype, destArchetype->count - 1,
                                       EntityRevalidationMode::ON_ADDED);
}

void World::MoveArchetype_Remove(EntityId eId, EntityRecord &r,
                                 Archetype *destArchetype)
{
    Archetype *srcArchetype = r.archetype;
    SwapBack(r);

    uint32_t removeRow = r.row;

    RevalidateCachedQuery_EntityFilter(srcArchetype, removeRow,
                                       EntityRevalidationMode::ON_REMOVED);

    if (!destArchetype)
    {
        if (srcArchetype->columnCount == 1 &&
            srcArchetype->componentSet.count == 1)
        {
            int32_t srcColIdx = srcArchetype->componentMap[0];
            Column &srcCol = srcArchetype->columns[srcColIdx];
            TypeInfo &ti = *srcCol.typeInfo;

            if (ti.HasData() && ti.hook.dtor)
            {
                void *src = OFFSET_ELEMENT(srcCol.data, ti.size,
                                           srcArchetype->count - 1);
                ti.hook.dtor(src);
            }

            r.row = 0;
            --srcArchetype->count;
            r.archetype = destArchetype;
        }
        else
        {
            assert(0);
        }
    }
    else
    {
        if (destArchetype->count == destArchetype->capacity)
        {
            GrowArchetype(*destArchetype);
        }

        for (size_t idx = 0; idx < srcArchetype->componentSet.count; idx++)
        {
            int32_t srcColIdx = srcArchetype->componentMap[idx];

            if (srcColIdx == -1)
            {
                continue;
            }

            Column &srcCol = srcArchetype->columns[srcColIdx];
            TypeInfo &ti = *srcCol.typeInfo;
            void *src = OFFSET(srcCol.data, ti.size * r.row);

            int32_t destIdx = destArchetype->componentSet.Search(
                srcArchetype->componentSet[idx]);
            if (destIdx != -1)
            {
                int32_t destColIdx = destArchetype->componentMap[destIdx];

                assert(destColIdx != -1);

                Column &destCol = destArchetype->columns[destColIdx];
                void *dest =
                    OFFSET(destCol.data, ti.size * destArchetype->count);

                if (ti.hook.moveCtor)
                {
                    ti.hook.moveCtor(dest, src);
                }
                else if (ti.hook.copyCtor)
                {
                    ti.hook.copyCtor(dest, src);
                }
                else
                {
                    std::memcpy(dest, src, ti.size);
                }
            }

            if (ti.hook.dtor)
            {
                ti.hook.dtor(src);
            }
        }

        destArchetype->entities[destArchetype->count] = eId;
        --srcArchetype->count;
        r.archetype = destArchetype;
        r.row = destArchetype->count;
        ++destArchetype->count;
    }

    RevalidateCachedQuery_EntityFilter(
        destArchetype, destArchetype == nullptr ? 0 : destArchetype->count - 1,
        EntityRevalidationMode::ON_ADDED);
}

void World::RevalidateCachedQuery_EntityFilter(Archetype *archetype,
                                               uint32_t affectedRow,
                                               EntityRevalidationMode mode)
{
    if (!archetype)
    {
        return;
    }

    // TODO:
    for (size_t qIdx = 0; qIdx < archetype->trackedQueries.count; ++qIdx)
    {
        TrackedQuery &trackedQuery = archetype->trackedQueries[qIdx];
        EcsQuery &q = Get<EcsQuery>(trackedQuery.id);

        if (!q.query->isEntityFiltered)
        {
            continue;
        }

        QueryArchetype &qAr = q.query->result[trackedQuery.idx];

        switch (mode)
        {
        case World::EntityRevalidationMode::ON_ADDED:
        {
            // Run entity filter on last entity
            qAr.AllocateMask(m_wAllocator);
            q.query->FilterEntity(qAr, affectedRow);
            break;
        }
        case World::EntityRevalidationMode::ON_REMOVED:
        {
            // Swap bitmask at count idx and remove idx (count is probably minus
            // 1 at this point)
            qAr.AllocateMask(m_wAllocator);
            bool bit = qAr.GetMask(qAr.archetype->count - 1);
            qAr.SetMask(qAr.archetype->count - 1, false);
            qAr.SetMask(affectedRow, bit);
            break;
        }
        case World::EntityRevalidationMode::ON_MODIFIED:
        {
            q.query->FilterEntity(qAr, affectedRow);
            break;
        }
        }
    }
}

void World::RevalidateCachedQuery_ArchetypeFilter(ComponentRecord &cr,
                                                  Archetype *archetype,
                                                  bool isNewArchetype)
{
    if (isNewArchetype)
    {
        for (size_t qIdx = 0; qIdx < cr.cachedQueries.count; ++qIdx)
        {
            EcsQuery &q = Get<EcsQuery>(cr.cachedQueries[qIdx]);
            if (q.query->lastSweep_archetype == m_q_revalSweep_archetype)
            {
                continue;
            }

            q.query->lastSweep_archetype = m_q_revalSweep_archetype;
            MatchedArchetype ma = q.query->IsMatch(archetype);

            if (ma.matched)
            {
                assert(ma.matchedColumns);
                archetype->trackedQueries.Add(
                    m_wAllocator,
                    TrackedQuery{q.query->eId, q.query->result.count});
                QueryArchetype qAr(archetype);
                qAr.SetMatchedColumnIdx(m_wAllocator, ma.matchedColumns,
                                        q.query->termCount);
                q.query->result.Add(m_wAllocator, std::move(qAr));
            }
        }
    }
    else
    {
        // TODO: handle cached query when remove archetype
    }
}


void World::Destroy()
{
    // clear archetype
    for (size_t aIdx = 1; aIdx <= m_archetypes.GetCount(); aIdx++)
    {
        Archetype *archetype =
            m_archetypes.GetPageData(m_archetypes.GetId(aIdx));
        assert(archetype);

        for (size_t cIdx = 0; cIdx < archetype->columnCount; cIdx++)
        {
            Column &col = archetype->columns[cIdx];
            TypeInfo &ti = *col.typeInfo;

            if (ti.hook.dtor)
            {
                for (uint32_t row = 0; row < archetype->count; row++)
                {
                    void *component = OFFSET(col.data, (ti.size * row));

                    ti.hook.dtor(component);
                }
            }

            m_wAllocator.Free(ti.size * archetype->capacity, col.data);
        }

        m_wAllocator.Free(sizeof(EntityId) * archetype->capacity,
                          archetype->entities);
        m_wAllocator.Free(sizeof(int32_t) * archetype->componentSet.count * 2,
                          archetype->componentMap);
        m_wAllocator.Free(sizeof(Column) * archetype->componentSet.count,
                          archetype->columns);
        archetype->addEdges.Destroy();
        archetype->removeEdges.Destroy();
    }

    m_archetypes.Destroy();
    m_entityIndex.Destroy();

    // NOTE: should clear the data if keeping metadata between world is
    // favorable
    m_componentIndex.Destroy();

    m_mappedArchetype.Destroy();
    m_typeInfos.Destroy();

    m_allocators.archetypes.Destroy();
    m_allocators.queries.Destroy();
    m_componentStore.Destroy(m_wAllocator);

    //for (uint32_t sIdx = 0; sIdx < m_systemStore.count; sIdx++)
    //{
    //    SystemCallback &sc = m_systemStore.store[sIdx];
    //    ArchetypeLinkedList *head = sc.archetypeList;

    //    while (head->archetype)
    //    {
    //        ArchetypeLinkedList *freeNode = head;
    //        head = head->next;

    //        ArchetypeLinkedList::Free(m_wAllocator, freeNode);
    //    }

    //    sc.componentSet.Destroy(m_wAllocator);
    //}
    //m_systemStore.Destroy(m_wAllocator);

    for (uint32_t bIdx = 1; bIdx <= m_wAllocator.m_sparse.GetCount(); bIdx++)
    {
        BlockAllocator *ba = m_wAllocator.m_sparse.GetPageData(
            m_wAllocator.m_sparse.GetId(bIdx));
        assert(ba);

        ba->Destroy();
    }

    m_wAllocator.m_sparse.Destroy();
    m_wAllocator.m_chunks.Destroy();
}

} // namespace ECS
