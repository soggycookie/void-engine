#include "world.h"
#include "ecs_type.h"
#include "ecs_utils.h"
#include "entity.h"
#include "internal_component.h"
#include "pipeline.h"
#include "query.h"
#include "type_info.h"
#include <cassert>

namespace ECS
{
World *CreateWorld()
{
    auto w = new World();
    w->Bootstrap();
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
    m_componentRecordIndex.Init(&m_wAllocator, 8);
    m_typeInfos.Init(&m_wAllocator, 8);
    m_setToArchetypes.Init(&m_wAllocator, 8);

    // m_systemStore.Init(m_wAllocator);
    m_componentStore.Init(m_wAllocator);
    m_isDeferred = false;
    m_loopPipeline.Init(this);

    RegisterInternalComponents();
    InitBasePhase();
}

void World::InitAllocators()
{
    m_allocators.archetypes.Init(sizeof(Archetype));
    m_allocators.queries.Init(sizeof(Query));
}

void World::RegisterInternalComponents()
{
    Component<EcsName>().Id(EcsNameId).Register();
    Component<EcsInherit>().Id(EcsInheritId).Register();
    Component<EcsSystem>()
        .Id(EcsSystemId)
        .MoveCtor(
            [](void *dest, void *src)
            {
                auto s =
                    new (dest) EcsSystem(std::move(*PTR_CAST(src, EcsSystem)));
                PTR_CAST(src, EcsSystem)->query = nullptr;
            })
        .Dtor(
            [](void *src)
            {
                EcsSystem *s = PTR_CAST(src, EcsSystem);
                if (s->query)
                {
                    std::cout << "Release Query" << std::endl;
                    s->query->Release();
                    s->~EcsSystem();
                }
            })
        .Register();

    Component<EcsQuery>()
        .Id(EcsQueryId)
        .MoveCtor(
            [](void *dest, void *src)
            {
                new (dest) EcsSystem(std::move(*PTR_CAST(src, EcsSystem)));
                PTR_CAST(src, EcsSystem)->query = nullptr;
            })
        .Dtor(
            [](void *src)
            {
                EcsQuery *s = PTR_CAST(src, EcsQuery);
                if (s->query)
                {
                    s->query->Release();
                    s->~EcsQuery();
                }
            })
        .Register();
    
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

void World::InitBasePhase()
{
    CreateEntityBuilder()
        .Id(EcsOnBootId)
        .Name(EcsOnBoot)
        .AddTag<EcsPhase>()
        .Build();

    CreateEntityBuilder()
        .Id(EcsOnLoopId)
        .Name(EcsOnLoop)
        .AddTag<EcsPhase>()
        .Build();
}

void World::InitDefaultPipelinePhase()
{
    CreateEntityBuilder()
        .Id(EcsOnStartId)
        .Name(EcsOnStart)
        .AddTag<EcsPhase>()
        .AddRelationship<EcsDependOn>(EcsOnBootId)
        .Build();

    CreateEntityBuilder()
        .Id(EcsOnValidationId)
        .Name(EcsOnValidation)
        .AddTag<EcsPhase>()
        .AddRelationship<EcsDependOn>(EcsOnLoopId)
        .Build();

    CreateEntityBuilder()
        .Id(EcsOnStartFrameId)
        .Name(EcsOnStartFrame)
        .AddTag<EcsPhase>()
        .AddRelationship<EcsDependOn>(EcsOnValidationId)
        .Build();

    CreateEntityBuilder()
        .Id(EcsOnPreUpdateId)
        .Name(EcsOnPreUpdate)
        .AddTag<EcsPhase>()
        .AddRelationship<EcsDependOn>(EcsOnStartFrameId)
        .Build();

    CreateEntityBuilder()
        .Id(EcsOnUpdateId)
        .Name(EcsOnUpdate)
        .AddTag<EcsPhase>()
        .AddRelationship<EcsDependOn>(EcsOnPreUpdateId)
        .Build();

    CreateEntityBuilder()
        .Id(EcsOnPostUpdateId)
        .Name(EcsOnPostUpdate)
        .AddTag<EcsPhase>()
        .AddRelationship<EcsDependOn>(EcsOnUpdateId)
        .Build();

    CreateEntityBuilder()
        .Id(EcsOnEndFrameId)
        .Name(EcsOnEndFrame)
        .AddTag<EcsPhase>()
        .AddRelationship<EcsDependOn>(EcsOnPostUpdateId)
        .Build();
}

// Register Type
// World do this because of letting TypeInfoBuilder do it seem messy
// return eId to make TypeInfoBuilder map Type -> eId (not work on relationship)
void World::Register(const TypeInfo &typeInfo, EntityId cId,
                     const std::string_view cName)
{
    TypeInfo *ti = new (m_wAllocator.Alloc(sizeof(TypeInfo))) TypeInfo();
    std::memcpy(ti, &typeInfo, sizeof(TypeInfo));

    size_t cNameLen = cName.size();

    if (cNameLen >= MaxEntityNameLength)
    {
        cNameLen = MaxEntityNameLength - 1;
    }

    char name[MaxEntityNameLength];
    std::memcpy(name, cName.data(), cNameLen);
    name[cNameLen] = '\0';

    if (m_componentStore.capacity == m_componentStore.count)
    {
        m_componentStore.Grow(m_wAllocator);
    }

    m_componentStore.Add(m_wAllocator, ti->id);

    ComponentRecord cr;
    cr.id = ti->id;
    cr.typeInfo = ti;

#ifdef ECS_DEBUG
    std::memcpy(cr.name, name, std::strlen(name));
#endif

    m_componentRecordIndex.Insert(ti->id, std::move(cr));
    m_typeInfos.Insert(ti->id, ti);

    auto builder = CreateEntityBuilder();
    builder.Name(name).Id(ti->id);

    if (ti->IsSingleton())
    {
        builder.AddComponent(ti->id);
    }

    builder.Build();
}

void World::RegisterRelationship(EntityId relationId, EntityId targetId,
                                 TypeInfo *relationTi)
{
    assert(relationTi);
    assert(targetId != EcsInvalidId && relationId != EcsInvalidId);
    assert(IsEntityExist(targetId) && IsEntityExist(relationId));

    EntityId relationshipId = MakeRelationship(relationId, targetId);

    if (m_componentStore.capacity == m_componentStore.count)
    {
        m_componentStore.Grow(m_wAllocator);
    }

    const EcsName &relName = Get<EcsName>(relationId);
    const EcsName &targetName = Get<EcsName>(targetId);

    size_t relNameSize = std::strlen(relName.name);
    size_t targetNameSize = std::strlen(targetName.name);

    // reserve last char for null terminator
    if (relNameSize >= MaxEntityNameLength)
    {
        relNameSize = MaxEntityNameLength - 1;
        targetNameSize = 0;
    }
    else
    {
        // minus one for the space between 2 names
        size_t maxSecondSize = (MaxEntityNameLength - 1) - relNameSize - 1;

        if (targetNameSize > maxSecondSize)
        {
            targetNameSize = maxSecondSize;
        }
    }

    char name[MaxEntityNameLength];
    std::memcpy(name, relName.name, relNameSize);

    if (targetNameSize != 0)
    {
        name[relNameSize] = ' ';
        std::memcpy(name + relNameSize + 1, targetName.name, targetNameSize);
        name[relNameSize + 1 + targetNameSize] = '\0';
    }
    else
    {
        name[relNameSize] = '\0';
    }

    Entity e = CreateEntityBuilder().Name(name).Build();

    ComponentRecord cr;
    cr.id = e.GetFullId();
    cr.typeInfo = relationTi;

#ifdef ECS_DEBUG
    std::memcpy(cr.name, name, strlen(name));
#endif

    m_componentRecordIndex.Insert(relationshipId, std::move(cr));
}

Entity World::CreateBaseEntity(EntityId eId)
{

    bool newId = false;
    if (!m_entityIndex.IsExisting(eId))
    {
        newId = true;
    }
    else
    {
        auto pair = GetResuedOrNewId();
        eId = pair.second;
        newId = pair.first;
    }

    uint32_t dense = m_entityIndex.PushBack(eId, EntityRecord{}, newId);
    EntityRecord &r = *m_entityIndex.GetPageData(eId);
    r.dense = dense;

    return Entity(this, eId);
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
    return CreateEntityBuilder()
        .Name(name)
        .Id(id)
        .AddRelationship<EcsChildOf>(parent)
        .Build();
}

EntityBuilder World::CreateEntityBuilder() { return EntityBuilder(this); }

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
    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);
    uint32_t dense = r->dense;
    uint64_t versionedId = m_entityIndex.GetDenseArr()[dense];

    return Entity(this, eId);
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
    if (!m_componentRecordIndex.ContainsKey(relationshipId))
    {
        return Store<EntityId>();
    }

    ComponentRecord &cr = m_componentRecordIndex[relationshipId];
    Store<EntityId> store;

    for (size_t idx = 0; idx < cr.archetypes.count; ++idx)
    {
        for (size_t eIdx = 0; eIdx < cr.archetypes[idx]->count; ++eIdx)
        {
            store.Add(m_wAllocator, cr.archetypes[idx]->entities[eIdx]);
        }
    }

    return store;
}

Archetype *World::GetEntityArchetype(EntityId eId)
{
    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    return r->archetype;
}

void World::RemoveEntity(EntityId eId)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);

    SwapBackEntity(*r);

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

void World::ResolveEntityCommand(EntityCommand &patch)
{
    if (m_isDestroying)
    {
        return;
    }

    assert(IsEntityExist(patch.eId));

    EntityRecord &r = *m_entityIndex.GetPageData(patch.eId);
    Archetype *srcArchetype = r.archetype;
    Archetype *destArchetype = r.archetype;

    for (size_t idx = 0; idx < patch.addCmds.count; ++idx)
    {
        AddCommand &cmd = patch.addCmds[idx];
        if (cmd.typeInfo->IsRelation())
        {
            if (!m_componentRecordIndex.ContainsKey(cmd.cId))
            {
                RegisterRelationship(LO_ENTITY_ID(cmd.cId),
                                     HI_ENTITY_ID(cmd.cId), cmd.typeInfo);
            }
        }

        destArchetype = GetOrCreateArchetype_Add(destArchetype, cmd.cId);
    }

    for (size_t idx = 0; idx < patch.removeCmds.count; ++idx)
    {
        destArchetype = GetOrCreateArchetype_Remove(destArchetype,
                                                    patch.removeCmds[idx].cId);
    }

    if (destArchetype && destArchetype->count == destArchetype->capacity)
    {
        GrowArchetype(*destArchetype);
    }

    if (srcArchetype == destArchetype)
    {
        return;
    }

    size_t srcCount =
        (srcArchetype == nullptr) ? 0 : srcArchetype->componentSet.count;
    size_t destCount =
        (destArchetype == nullptr) ? 0 : destArchetype->componentSet.count;

    size_t count = std::max(srcCount, destCount);

    size_t srcIdx = 0;
    size_t destIdx = 0;

    size_t addIdx = 0;
    size_t removeIdx = 0;

    size_t addCmdCount = patch.addCmds.count;
    size_t removeCmdCount = patch.removeCmds.count;
    size_t hasAddCmd = addCmdCount > 0;
    size_t hasRemoveCmd = removeCmdCount > 0;

    if(srcArchetype)
    {
        SwapBackEntity(r);
    }

    while (true)
    {
        bool s = false;
        bool d = false;

        EntityId srcCId = EcsInvalidId;
        if (srcIdx < srcCount)
        {
            srcCId = srcArchetype->componentSet[srcIdx];
            s = true;
        }

        EntityId destCId = EcsInvalidId;
        if (destIdx < destCount)
        {
            destCId = destArchetype->componentSet[destIdx];
            d = true;
        }

        if (!s && !d)
        {
            break;
        }

        if (srcCId == destCId && srcCId != EcsInvalidId)
        {
            // Move row
            int32_t destColIdx = destArchetype->columnMap[destIdx];

            if(destColIdx == ComponentSet::NotFoundIdx)
            {
                ++srcIdx;
                ++destIdx;
                continue;
            }

            Column &destCol = destArchetype->columns[destColIdx];
            TypeInfo *destTi = destCol.typeInfo;
            void *destData = OFFSET_ELEMENT(destCol.data, destTi->size,
                                            destArchetype->count);

            int32_t srcColIdx = srcArchetype->columnMap[srcIdx];
            assert(srcColIdx != ComponentSet::NotFoundIdx);
            Column &srcCol = srcArchetype->columns[srcColIdx];
            TypeInfo *srcTi = srcCol.typeInfo;

            void *srcData = OFFSET_ELEMENT(srcCol.data, srcTi->size,
                                           srcArchetype->count - 1);

            if (srcTi->hook.moveCtor)
            {
                srcTi->hook.moveCtor(destData, srcData);
            }
            else if (srcTi->hook.copyCtor)
            {
                srcTi->hook.copyCtor(destData, srcData);
            }
            else
            {
                std::memcpy(destData, srcData, srcTi->size);
            }

            if (srcTi->hook.dtor)
            {
                srcTi->hook.dtor(srcData);
            }
            ++srcIdx;
            ++destIdx;
        }
        else
        {
            if (destArchetype && hasAddCmd && addIdx < addCmdCount &&
                destCId == patch.addCmds[addIdx].cId)
            {
                AddCommand &cmd = patch.addCmds[addIdx];

                int32_t colIdx = destArchetype->columnMap[destIdx];
                
                if(colIdx == ComponentSet::NotFoundIdx)
                {
                    ++destIdx;
                    ++addIdx;
                    continue;
                }
                
                Column &col = destArchetype->columns[colIdx];
                TypeInfo *ti = col.typeInfo;

                void *destData =
                    OFFSET_ELEMENT(col.data, ti->size, destArchetype->count);

                switch (cmd.type)
                {
                case ComponentAddMode::ADD_TYPE:
                {
                    assert(!cmd.data);
                    if (ti->hook.ctor)
                    {
                        ti->hook.ctor(destData);
                    }
                    else
                    {
                        assert(0);
                    }

                    break;
                }
                case ComponentAddMode::ASSIGN_CONST_TYPE:
                {
                    assert(cmd.data);
                    if (ti->hook.copyCtor)
                    {
                        ti->hook.copyCtor(destData, cmd.data);
                    }
                    else
                    {
                        std::memcpy(destData, cmd.data, ti->size);
                    }
                    break;
                }
                case ComponentAddMode::ASSIGN_MUT_TYPE:
                {
                    assert(cmd.data);
                    if (ti->hook.moveCtor)
                    {
                        ti->hook.moveCtor(destData, cmd.data);
                    }
                    else if (ti->hook.copyCtor)
                    {
                        ti->hook.copyCtor(destData, cmd.data);
                    }
                    else
                    {
                        std::memcpy(destData, cmd.data, ti->size);
                    }

                    break;
                }
                default:
                {
                    assert(0);
                }
                }

                // free temp data
                if (cmd.data)
                {
                    if(ti->hook.dtor)
                    {
                        ti->hook.dtor(cmd.data);
                    }
                    m_wAllocator.Free(ti->size, cmd.data);
                }

                ++destIdx;
                ++addIdx;
            }
            else if (srcArchetype && hasRemoveCmd &&
                     removeIdx < removeCmdCount &&
                     srcCId == patch.removeCmds[removeIdx].cId)
            {
                int32_t colIdx = srcArchetype->columnMap[srcIdx];

                if(colIdx == ComponentSet::NotFoundIdx)
                {
                    ++srcIdx;
                    ++removeIdx;
                    continue;
                }

                Column &col = srcArchetype->columns[colIdx];
                TypeInfo *ti = col.typeInfo;

                void *srcData =
                    OFFSET_ELEMENT(col.data, ti->size, srcArchetype->count - 1);

                if (ti->hook.dtor)
                {
                    ti->hook.dtor(srcData);
                }
                ++srcIdx;
                ++removeIdx;
            }
            else
            {
                assert(0 && "Unknown case!");
            }
        }
    }

    r.archetype = destArchetype;
    r.row = 0;

    if (srcArchetype)
    {
        RevalidateCachedQuery_EntityFilter(srcArchetype, --srcArchetype->count, EntityRevalidationMode::ON_REMOVED);
    }

    if (destArchetype)
    {
        destArchetype->entities[destArchetype->count] = patch.eId;
        r.row = destArchetype->count;
        RevalidateCachedQuery_EntityFilter(destArchetype, destArchetype->count++, EntityRevalidationMode::ON_ADDED);
    }
}

void World::AddComponent(EntityId eId, EntityId cId)
{
    if (m_isDestroying)
    {
        return;
    }

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

    // if (m_isDeferred)
    //{
    //     EntityDeferredCommand cmd;
    //     cmd.id = eId;
    //     cmd.addCmd = AddCommand{cId, nullptr};
    //     cmd.mode = AddCmdMode;
    //     cmd.typeInfo = ti;

    //    m_deferredCmds.Add(m_wAllocator, cmd);
    //}
    // else
    {
        AddInternal(eId, cId, r, ti);
    }
}

void World::AssignComponent(EntityId eId, EntityId cId, void *data)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[cId];

    assert(ti->IsComponent());
    assert(r);
    assert(!(r->archetype->componentSet.Has(cId)));

    /* if (m_isDeferred)
     {
         EntityDeferredCommand cmd;
         cmd.id = eId;

         void *deferredData = m_wAllocator.Alloc(ti->size);

         if (ti->hook.moveCtor)
         {
             ti->hook.moveCtor(deferredData, data);
         }
         else if (ti->hook.copyCtor)
         {
             ti->hook.copyCtor(deferredData, data);
         }
         else
         {
             std::memcpy(deferredData, data, ti->size);
         }

         cmd.addCmd = AddCommand{cId, deferredData};
         cmd.mode = AddCmdMode;
         cmd.typeInfo = ti;

         m_deferredCmds.Add(m_wAllocator, cmd);
     }
     else*/
    {

        Archetype *destArchetype = GetOrCreateArchetype_Add(r->archetype, cId);

        MoveArchetype_Set(eId, *r, destArchetype, data);
    }
}

void World::AssignComponent(EntityId eId, EntityId cId, const void *data)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[cId];

    assert(ti->IsComponent());
    assert(r);
    assert(!(r->archetype->componentSet.Has(cId)));

    /* if (m_isDeferred)
     {
         EntityDeferredCommand cmd;
         cmd.id = eId;

         void *deferredData = m_wAllocator.Alloc(ti->size);

         if (ti->hook.copyCtor)
         {
             ti->hook.copyCtor(deferredData, data);
         }
         else
         {
             std::memcpy(deferredData, data, ti->size);
         }

         cmd.addCmd = AddCommand{cId, deferredData};
         cmd.mode = AddCmdMode;
         cmd.typeInfo = ti;

         m_deferredCmds.Add(m_wAllocator, cmd);
     }
     else*/
    {

        Archetype *destArchetype = GetOrCreateArchetype_Add(r->archetype, cId);

        MoveArchetype_Set(eId, *r, destArchetype, data);
    }
}

void World::AddRelationship(EntityId eId, EntityId relationId,
                            EntityId targetId)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId) || IsEntityVersionOutdated(targetId))
    {
        assert(0);
    }

    EntityId relationshipId = MakeRelationship(relationId, targetId);

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[relationId];

    assert(ti->IsRelation());
    assert(r);
    assert(!r->archetype->componentSet.Has(relationshipId));

    if (ti->IsExclusive())
    {
        assert(!r->archetype->componentSet.HasRelationship(relationId));
    }

    /*if (m_isDeferred)
    {

        EntityDeferredCommand cmd;
        cmd.id = eId;

        cmd.addCmd = AddCommand{relationshipId, nullptr};
        cmd.mode = AddCmdMode;
        cmd.typeInfo = ti;

        m_deferredCmds.Add(m_wAllocator, cmd);
    }
    else*/
    {

        if (!m_componentRecordIndex.ContainsKey(relationshipId))
        {
            RegisterRelationship(relationId, targetId, ti);
        }

        AddInternal(eId, relationshipId, r, ti);
    }
}

void World::AssignRelationship(EntityId eId, EntityId relationId,
                               EntityId targetId, void *data)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId) || IsEntityVersionOutdated(targetId))
    {
        assert(0);
    }

    EntityId relationshipId = MakeRelationship(relationId, targetId);

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[relationId];

    assert(ti->IsRelation());
    assert(ti->HasData());
    assert(r);
    assert(!r->archetype->componentSet.Has(relationshipId));

    if (ti->IsExclusive())
    {
        assert(!r->archetype->componentSet.HasRelationship(relationId));
    }

    /*if (m_isDeferred)
    {

        EntityDeferredCommand cmd;
        cmd.id = eId;

        void *deferredData = m_wAllocator.Alloc(ti->size);

        if (ti->hook.moveCtor)
        {
            ti->hook.moveCtor(deferredData, data);
        }
        else if (ti->hook.copyCtor)
        {
            ti->hook.copyCtor(deferredData, data);
        }
        else
        {
            std::memcpy(deferredData, nullptr, ti->size);
        }

        cmd.addCmd = AddCommand{relationshipId, deferredData};
        cmd.mode = AddCmdMode;
        cmd.typeInfo = ti;

        m_deferredCmds.Add(m_wAllocator, cmd);
    }
    else*/
    {

        if (!m_componentRecordIndex.ContainsKey(relationshipId))
        {
            RegisterRelationship(relationId, targetId, ti);
        }

        Archetype *destArchetype =
            GetOrCreateArchetype_Add(r->archetype, relationshipId);

        MoveArchetype_Set(eId, *r, destArchetype, data);
    }
}

void World::AssignRelationship(EntityId eId, EntityId relationId,
                               EntityId targetId, const void *data)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId) || IsEntityVersionOutdated(targetId))
    {
        assert(0);
    }

    EntityId relationshipId = MakeRelationship(relationId, targetId);

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[relationId];

    assert(ti->IsRelation());
    assert(ti->HasData());
    assert(r);
    assert(!r->archetype->componentSet.Has(relationshipId));

    if (ti->IsExclusive())
    {
        assert(!r->archetype->componentSet.HasRelationship(relationId));
    }

    /*if (m_isDeferred)
    {

        EntityDeferredCommand cmd;
        cmd.id = eId;

        void *deferredData = m_wAllocator.Alloc(ti->size);

        if (ti->hook.copyCtor)
        {
            ti->hook.copyCtor(deferredData, data);
        }
        else
        {
            std::memcpy(deferredData, nullptr, ti->size);
        }

        cmd.addCmd = AddCommand{relationshipId, deferredData};
        cmd.mode = AddCmdMode;
        cmd.typeInfo = ti;

        m_deferredCmds.Add(m_wAllocator, cmd);
    }
    else*/
    {

        if (!m_componentRecordIndex.ContainsKey(relationshipId))
        {
            RegisterRelationship(relationId, targetId, ti);
        }

        Archetype *destArchetype =
            GetOrCreateArchetype_Add(r->archetype, relationshipId);

        MoveArchetype_Set(eId, *r, destArchetype, data);
    }
}

void World::AddTag(EntityId eId, EntityId cId)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    TypeInfo *ti = m_typeInfos[cId];

    assert(r);
    assert(ti->IsTag());
    assert(!r->archetype->componentSet.Has(cId));

    /*if (m_isDeferred)
    {
        EntityDeferredCommand cmd;
        cmd.id = eId;
        cmd.addCmd = AddCommand{cId, nullptr};
        cmd.mode = AddCmdMode;
        cmd.typeInfo = ti;

        m_deferredCmds.Add(m_wAllocator, cmd);
    }
    else*/
    {
        AddInternal(eId, cId, r, ti);
        if (ti->hook.onAdd)
        {
            ti->hook.onAdd(nullptr, this);
        }
    }
}

void World::AddInternal(EntityId eId, EntityId cId, EntityRecord *r,
                        TypeInfo *ti)
{
    assert(!ti->IsSingleton());

    Archetype *destArchetype = GetOrCreateArchetype_Add(r->archetype, cId);

    MoveArchetype_Add(eId, *r, destArchetype);
}

void World::RemoveComponent(EntityId eId, EntityId cId)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    Archetype *srcArchetype = r->archetype;
    TypeInfo *ti = m_typeInfos[cId];

    assert(r);
    assert(r->archetype);
    assert(r->archetype->componentSet.Has(cId));

    // if (m_isDeferred)
    //{
    //     EntityDeferredCommand cmd;
    //     cmd.id = eId;
    //     cmd.mode = CmdMode::REMOVE_CMD_MODE;
    //     cmd.removeCmds = RemoveCommand{cId};
    //     cmd.typeInfo = ti;

    //    m_deferredCmds.Add(m_wAllocator, cmd);
    //}
    // else
    {
        Archetype *destArchetype =
            GetOrCreateArchetype_Remove(srcArchetype, cId);

        MoveArchetype_Remove(eId, *r, destArchetype);

        if (ti->IsTag() && ti->hook.onRemove)
        {
            ti->hook.onRemove(nullptr, this);
        }
    }
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
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);
    assert(r->archetype);
    assert(r->dense);

    int32_t idx = r->archetype->componentSet.Search(cId);
    assert(idx != ComponentSet::NotFoundIdx);

    int32_t colIdx = r->archetype->columnMap[idx];
    assert(colIdx != ComponentSet::NotFoundIdx);

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

    ti->hook.onSet(dest, this);
}

void World::Set(EntityId eId, EntityId cId, const void *data)
{
    if (m_isDestroying)
    {
        return;
    }

    if (IsEntityVersionOutdated(eId))
    {
        assert(0);
    }

    EntityRecord *r = m_entityIndex.GetPageData(eId);
    assert(r);
    assert(r->archetype);
    assert(r->dense);

    int32_t idx = r->archetype->componentSet.Search(cId);
    assert(idx != ComponentSet::NotFoundIdx);

    int32_t colIdx = r->archetype->columnMap[idx];
    assert(colIdx != ComponentSet::NotFoundIdx);

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

    ti->hook.onSet(dest, this);
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
    int32_t colIdx = r->archetype->columnMap[idx];
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
        PTR_CAST(m_wAllocator.Alloc(sizeof(EntityId) * newCapacity), EntityId);

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
        void *newColData = m_wAllocator.Alloc(ti->size * newCapacity);

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

void World::SwapBackEntity(EntityRecord &r)
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

        void *temp = m_wAllocator.Alloc(ti.size);

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
        m_wAllocator.Alloc(sizeof(Column) * componentSet.count), Column);
    archetype.entities = PTR_CAST(
        m_wAllocator.Alloc(sizeof(EntityId) * DefaultArchetypeCapacity),
        EntityId);
    archetype.columnMap = PTR_CAST(
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
                m_wAllocator.Alloc(ti->size * DefaultArchetypeCapacity);

            archetype.columnMap[idx] = dataColCounter;
            archetype.columnMap[componentSet.count + dataColCounter] = idx;
            ++dataColCounter;
        }
        else
        {
            archetype.columnMap[idx] = -1;
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
        ComponentRecord &cr = m_componentRecordIndex[componentSet[idx]];

        RevalidateCachedQuery_ArchetypeFilter(cr, rArchetype, true);

        // union pair
        if (cr.typeInfo->IsRelation())
        {
            ComponentRecord &pCr =
                m_componentRecordIndex[LO_ENTITY_ID(componentSet[idx])];

            RevalidateCachedQuery_ArchetypeFilter(pCr, rArchetype, true);

            if (pCr.archetypes.count == pCr.archetypes.capacity)
            {
                pCr.archetypes.Grow(m_wAllocator);
            }

            pCr.archetypes.store[pCr.archetypes.count] = rArchetype;
            ++pCr.archetypes.count;
        }

        if (cr.archetypes.count == cr.archetypes.capacity)
        {
            cr.archetypes.Grow(m_wAllocator);
        }

        cr.archetypes.store[cr.archetypes.count] = rArchetype;
        ++cr.archetypes.count;
    }

    m_setToArchetypes.Insert(std::move(componentSet), rArchetype);

    return rArchetype;
}

Archetype *World::GetArchetype(const ComponentSet &componentSet)
{
    if (m_setToArchetypes.ContainsKey(componentSet))
    {
        return m_setToArchetypes[componentSet];
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
            void *dest = OFFSET(destArchetype->columns[0].data,
                                destArchetype->count * ti.size);

            assert(ti.hook.ctor);
            ti.hook.ctor(dest);

            if (ti.hook.onAdd)
            {
                ti.hook.onAdd(dest, this);
            }
        }
    }
    // at least 1 component
    else
    {
        // SWAP BACK IN SRC ARCHETYPE
        SwapBackEntity(r);

        for (size_t idx = 0; idx < destArchetype->componentSet.count; idx++)
        {
            // skip no data tag and relationship
            int32_t destColIdx = destArchetype->columnMap[idx];

            if (destColIdx == ComponentSet::NotFoundIdx)
            {
                continue;
            }

            Column &destCol = destArchetype->columns[destColIdx];
            TypeInfo &ti = *destCol.typeInfo;

            void *dest = OFFSET(destCol.data, ti.size * destArchetype->count);

            int32_t srcIndex = srcArchetype->componentSet.Search(
                destArchetype->componentSet[idx]);

            if (srcIndex == ComponentSet::NotFoundIdx)
            {
                // find the newly added component
                assert(ti.hook.ctor);
                ti.hook.ctor(dest);
                if (ti.hook.onAdd)
                {
                    ti.hook.onAdd(dest, this);
                }
            }
            else
            {
                int32_t srcColIdx = srcArchetype->columnMap[srcIndex];

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

void World::MoveArchetype_Set(EntityId eId, EntityRecord &r,
                              Archetype *destArchetype, void *data)
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
            void *dest = OFFSET(destArchetype->columns[0].data,
                                destArchetype->count * ti.size);

            if (!data)
            {
                assert(ti.hook.ctor);
                ti.hook.ctor(dest);

                if (ti.hook.onAdd)
                {
                    ti.hook.onAdd(dest, this);
                }
            }
            else
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

                if (ti.hook.onAdd)
                {
                    ti.hook.onAdd(dest, this);
                }
                if (ti.hook.onSet)
                {
                    ti.hook.onSet(dest, this);
                }
            }
        }
    }
    // at least 1 component
    else
    {
        // SWAP BACK IN SRC ARCHETYPE
        SwapBackEntity(r);

        for (size_t idx = 0; idx < destArchetype->componentSet.count; idx++)
        {
            // skip no data tag and relationship
            int32_t destColIdx = destArchetype->columnMap[idx];

            if (destColIdx == -1)
            {
                continue;
            }

            Column &destCol = destArchetype->columns[destColIdx];
            TypeInfo &ti = *destCol.typeInfo;

            void *dest = OFFSET(destCol.data, ti.size * destArchetype->count);

            int32_t srcIndex = srcArchetype->componentSet.Search(
                destArchetype->componentSet[idx]);

            if (srcIndex == ComponentSet::NotFoundIdx)
            {
                // find the newly added component
                if (!data)
                {
                    assert(ti.hook.ctor);
                    ti.hook.ctor(dest);

                    if (ti.hook.onAdd)
                    {
                        ti.hook.onAdd(dest, this);
                    }
                }
                else
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

                    if (ti.hook.onAdd)
                    {
                        ti.hook.onAdd(dest, this);
                    }
                    if (ti.hook.onSet)
                    {
                        ti.hook.onSet(dest, this);
                    }
                }
            }
            else
            {
                int32_t srcColIdx = srcArchetype->columnMap[srcIndex];

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

void World::MoveArchetype_Set(EntityId eId, EntityRecord &r,
                              Archetype *destArchetype, const void *data)
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
            void *dest = OFFSET(destArchetype->columns[0].data,
                                destArchetype->count * ti.size);

            if (!data)
            {
                assert(ti.hook.ctor);
                ti.hook.ctor(dest);

                if (ti.hook.onAdd)
                {
                    ti.hook.onAdd(dest, this);
                }
            }
            else
            {
                if (ti.hook.copyCtor)
                {
                    ti.hook.copyCtor(dest, data);
                }
                else
                {
                    std::memcpy(dest, data, ti.size);
                }

                if (ti.hook.onAdd)
                {
                    ti.hook.onAdd(dest, this);
                }
                if (ti.hook.onSet)
                {
                    ti.hook.onSet(dest, this);
                }
            }
        }
    }
    // at least 1 component
    else
    {
        // SWAP BACK IN SRC ARCHETYPE
        SwapBackEntity(r);

        for (size_t idx = 0; idx < destArchetype->componentSet.count; idx++)
        {
            // skip no data tag and relationship
            int32_t destColIdx = destArchetype->columnMap[idx];

            if (destColIdx == -1)
            {
                continue;
            }

            Column &destCol = destArchetype->columns[destColIdx];
            TypeInfo &ti = *destCol.typeInfo;

            void *dest = OFFSET(destCol.data, ti.size * destArchetype->count);

            int32_t srcIndex = srcArchetype->componentSet.Search(
                destArchetype->componentSet[idx]);

            if (srcIndex == ComponentSet::NotFoundIdx)
            {
                // find the newly added component
                if (!data)
                {
                    assert(ti.hook.ctor);
                    ti.hook.ctor(dest);

                    if (ti.hook.onAdd)
                    {
                        ti.hook.onAdd(dest, this);
                    }
                }
                else
                {
                    if (ti.hook.copyCtor)
                    {
                        ti.hook.copyCtor(dest, data);
                    }
                    else
                    {
                        std::memcpy(dest, data, ti.size);
                    }

                    if (ti.hook.onAdd)
                    {
                        ti.hook.onAdd(dest, this);
                    }
                    if (ti.hook.onSet)
                    {
                        ti.hook.onSet(dest, this);
                    }
                }
            }
            else
            {
                int32_t srcColIdx = srcArchetype->columnMap[srcIndex];

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
    SwapBackEntity(r);

    uint32_t removeRow = r.row;

    RevalidateCachedQuery_EntityFilter(srcArchetype, removeRow,
                                       EntityRevalidationMode::ON_REMOVED);

    if (!destArchetype)
    {
        if (srcArchetype->columnCount == 1)
        {
            int32_t srcColIdx = srcArchetype->columnMap[0];
            assert(srcColIdx != ComponentSet::NotFoundIdx);

            Column &srcCol = srcArchetype->columns[srcColIdx];
            TypeInfo &ti = *srcCol.typeInfo;

            void *src =
                OFFSET_ELEMENT(srcCol.data, ti.size, srcArchetype->count - 1);

            if (ti.hook.onRemove)
            {
                ti.hook.onRemove(src, this);
            }

            if (ti.hook.dtor)
            {
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
            int32_t srcColIdx = srcArchetype->columnMap[idx];

            if (srcColIdx == ComponentSet::NotFoundIdx)
            {
                continue;
            }

            Column &srcCol = srcArchetype->columns[srcColIdx];
            TypeInfo &ti = *srcCol.typeInfo;
            void *src = OFFSET(srcCol.data, ti.size * r.row);

            int32_t destIdx = destArchetype->componentSet.Search(
                srcArchetype->componentSet[idx]);
            if (destIdx != ComponentSet::NotFoundIdx)
            {
                int32_t destColIdx = destArchetype->columnMap[destIdx];

                assert(destColIdx != ComponentSet::NotFoundIdx);

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
            else
            {
                if (ti.hook.onRemove)
                {
                    ti.hook.onRemove(src, this);
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
            // Swap bitmask at count idx and remove idx (count is probably
            // minus 1 at this point)
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
        for (size_t qIdx = 0; qIdx < cr.trackedQueries.count; ++qIdx)
        {
            EcsQuery &q = Get<EcsQuery>(cr.trackedQueries[qIdx]);
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

PhaseDependencyBuilder World::BootstrapPhase()
{
    return PhaseDependencyBuilder(this, EcsOnBootId);
}

PhaseDependencyBuilder World::LoopPhase()
{
    return PhaseDependencyBuilder(this, EcsOnLoopId);
}

void World::Tick()
{
    m_isDeferred = true;

    if (!m_isFirstFrame)
    {
        Pipeline bootPipeline;
        bootPipeline.Init(this);
        bootPipeline.BuildFromBasePhase(EcsOnBootId);
        bootPipeline.Progress();

        m_loopPipeline.BuildFromBasePhase(EcsOnLoopId);
        m_isFirstFrame = true;

        bootPipeline.Destroy();
    }
    else
    {
        m_loopPipeline.Progress();
    }

    m_isDeferred = false;
}

void World::Destroy()
{
    m_isDestroying = true;
    m_loopPipeline.Destroy();

    // clear archetype
    for (size_t aIdx = 1; aIdx <= m_archetypes.GetCount(); aIdx++)
    {
        Archetype *archetype =
            m_archetypes.GetPageData(m_archetypes.GetId(aIdx));
        assert(archetype);

        for (size_t cIdx = 0; cIdx < archetype->columnCount; cIdx++)
        {
            int32_t colIdx = archetype->columnMap[cIdx];

            if (colIdx == ComponentSet::NotFoundIdx)
            {
                continue;
            }

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
                          archetype->columnMap);
        m_wAllocator.Free(sizeof(Column) * archetype->componentSet.count,
                          archetype->columns);
        archetype->addEdges.Destroy();
        archetype->removeEdges.Destroy();
    }

    m_archetypes.Destroy();
    m_entityIndex.Destroy();

    m_componentRecordIndex.Destroy();

    m_setToArchetypes.Destroy();
    m_typeInfos.Destroy();

    m_allocators.archetypes.Destroy();
    m_allocators.queries.Destroy();
    m_componentStore.Destroy(m_wAllocator);

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
