#include "world.h"
#include "ecs_type.h"
#include "ecs_utils.h"
#include "internal_component.h"
#include <cstdio>
#include <cstring>
#include <ostream>
#include <string_view>

namespace ECS
{
    World* CreateWorld()
    {
        auto w = new World();
        w->Init();
        //w->CreateInternalEntity();
        w->RegisterInternalComponents();
        return w;
    }

    void DestroyWorld(World* world)
    {
        world->Destroy();
        delete world;
    }

    void World::Init()
    {
        m_wAllocator.Init();
        InitAllocators();
        m_entityIndex.Init(&m_wAllocator, nullptr, 8, true);
        m_archetypes.Init(&m_wAllocator, &m_allocators.archetypes, 8, false);
        m_componentIndex.Init(&m_wAllocator, 8);
        m_typeInfos.Init(&m_wAllocator, 8);
        m_mappedArchetype.Init(&m_wAllocator, 8);

        m_systemStore.Init(m_wAllocator);
        m_componentStore.Init(m_wAllocator);
        m_isDefered = false;
    }

    void World::InitAllocators()
    {
        m_allocators.archetypes.Init(SparsePageCount * sizeof(Archetype));
    }

    void World::RegisterInternalComponents()
    {
        Component<EcsName>().Id(EcsNameId).Register();
        Component<EcsInherit>().Id(EcsInheritId).Register();

        Tag<EcsSystem>().Id(EcsSystemId).Register();
        Tag<EcsPhase>().Id(EcsPhaseId).Register();
        Tag<EcsArchetype>().Id(EcsArchetypeId).Register();
        Tag<EcsPipeline>().Id(EcsPipelineId).Register();
        Tag<EcsDisabled>().Id(EcsDisabledId).Register();

        Relation<EcsChildOf>().Id(EcsChildOfId).Exclusive().Register();
        Relation<EcsDependOn>().Id(EcsDependOnId).Register();
        Relation<EcsToggle>().Id(EcsToggleId).Register();
        Relation<EcsIsA>().Id(EcsIsAId).Register();
    }

    //Register Type
    //World do this because of letting TypeInfoBuilder do it seem messy
    //return eId to make TypeInfoBuilder map Type -> eId (not work on relationship)
    void World::Register(const TypeInfo& typeInfo, EntityId relationId, EntityId targetId, const std::string_view first, const std::string_view second)
    {
        TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
        std::memcpy(ti, &typeInfo, sizeof(TypeInfo));
    
        size_t firstSize = first.size();
        size_t secondSize = second.size();
        
        //reserve last char for null terminator
        if(firstSize >= MaxEntityNameLength)
        {
            firstSize = MaxEntityNameLength - 1;
            secondSize = 0;
        }
        else 
        {
            //minus one for the space between 2 names
            size_t maxSecondSize = (MaxEntityNameLength - 1) - firstSize - 1;

            if(secondSize > maxSecondSize)
            {
                secondSize = maxSecondSize;
            }
        }
        
        char name[MaxEntityNameLength];
        std::memcpy(name, first.data(), firstSize);
        
        if(secondSize != 0)
        {
            name[firstSize] = ' ';
            std::memcpy(name + firstSize + 1, second.data(), secondSize);
            name[firstSize + 1 + secondSize] = '\0';
        }
        else
        {
            name[firstSize] = '\0';
        }

        if(m_componentStore.capacity == m_componentStore.count)
        {
            m_componentStore.Grow(m_wAllocator);
        }
        
        if(ti->IsRelationship())
        {
            ti->cId = MakeRelationship(relationId, targetId);
        }
        else 
        {
            ti->cId = ti->eId;
            m_componentStore.Add(ti->cId);
        }

        ComponentRecord cr;
        cr.id = ti->eId;
        cr.typeInfo = ti;
        cr.archetypeStore.Init(m_wAllocator);

        assert(cr.archetypeStore.store);

        m_componentIndex.Insert(ti->cId, std::move(cr));
        m_typeInfos.Insert(ti->cId, ti);
        
        Entity e = CreateEntity(ti->eId, name, 0);
        // if(ti->eId != EcsNameId)
        // {
        //     Entity e = CreateEntity(ti->eId, name, 0);
        // }
        // else
        // {
        //     Entity e = CreateEntity(ti->eId, 0);
        //     char* n = PTR_CAST(m_wAllocator.Alloc(MaxEntityNameLength), char);
        //     std::memcpy(n, name, MaxEntityNameLength);
        //     e.AddComponent<EcsName>();
        //     e.Set<EcsName>(EcsName{n});
        // }
    }

    Entity World::CreateEntity(const char* name, EntityId parent)
    {
        return CreateEntity(0, name, parent);
    }

    Entity World::CreateEntity(char* name, EntityId parent)
    {
        return CreateEntity(0, name, parent);
    }

    Entity World::CreateEntity(EntityId id, const char* name, EntityId parent)
    {
        bool newId = false;
        if(!m_entityIndex.isValidDense(id))
        {
            newId = true;
        }
        else
        {
            auto pair = GetId();
            id = pair.second;
            newId = pair.first;
        }

        uint32_t dense = m_entityIndex.PushBack(id, EntityRecord{}, newId);
        EntityRecord& r = *m_entityIndex.GetPageData(id);
        r.dense = dense;

        char* entityName = nullptr;


        entityName = PTR_CAST(m_wAllocator.Alloc(MaxEntityNameLength), char);
        if(!name)
        {
            std::snprintf(entityName, MaxEntityNameLength, DefaultEntityName, LO_ENTITY_ID(id));
        }
        else
        {
            size_t len = std::strlen(name);
            if(len >= MaxEntityNameLength)
            {
                len = MaxEntityNameLength - 1;
                //NOTE:
                //WANRING
            }

            std::memcpy(entityName, name, len);
            entityName[len] = '\0';
        }


        EntityDesc desc;
        desc.id = id;
        desc.name = entityName;
        desc.parent = parent;
        desc.add.count = 0;

        ResolveEntityDesc(r, desc);

        return Entity(id, this);
    }

    bool World::IsEntityExist(EntityId eId)
    {
        return m_entityIndex.isValidDense(eId);
    }

    EntityId World::GetNewId()
    {
        while(m_entityIndex.isValidDense(++m_nextFreeId));

        return m_nextFreeId;
    }

    EntityId World::GetReusedId()
    {
        return m_entityIndex.GetReusedId();
    }

    std::pair<bool, EntityId> World::GetId()
    {
        bool newId = false;

        EntityId id = GetReusedId();

        if(id == 0)
        {
            newId = true;

            id = GetNewId();
        }
        else
        {
            INCRE_GEN_COUNT(id);
        }

        return {newId, id};
    }

    Entity World::GetEntity(EntityId eId)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);
        uint32_t dense = r->dense;
        uint64_t versionedId = m_entityIndex.GetDenseArr()[dense];
        if(!r || versionedId != eId)
        {
            return Entity(0, this);
        }

        return Entity(eId, this);
    }

    Entity World::CreateEntity(EntityDesc& desc)
    {
        bool newId = false;
        if(IsEntityExist(desc.id))
        {
            auto pair = GetId();
            desc.id = pair.second;
            newId = pair.first;
        }
        else
        {
            newId = true;
        }

        uint32_t dense = m_entityIndex.PushBack(desc.id, EntityRecord{}, newId);
        EntityRecord& r = *m_entityIndex.GetPageData(desc.id);
        r.dense = dense;

        ResolveEntityDesc(r, desc);

        return Entity(desc.id, this);
    }

    EntityRecord* World::GetEntityRecord(EntityId eId)
    {
        EntityRecord* e = m_entityIndex.GetPageData(eId);

        if(!e)
        {
            return nullptr;
        }
        else
        {
            return e;
        }
    }

    void World::ResolveEntityDesc(EntityRecord& r, EntityDesc& desc)
    {
        assert(desc.name);
        desc.add.Sort();

        Archetype* destArchetype = r.archetype;
        if(desc.parent != 0)
        {
            if(!m_componentIndex.ContainsKey(MakeRelationship(ComponentTypeId<EcsChildOf>::Id(), desc.parent)))
            {
                TypeInfoBuilder<EcsChildOf> tiBuilder(this);
                tiBuilder.Relationship(desc.parent).Register();
            }

            destArchetype = GetOrCreateArchetype_Add(destArchetype, MakeRelationship(ComponentTypeId<EcsChildOf>::Id(), desc.parent));
        }
       
        //if(desc.id != ComponentTypeId<EcsName>::Id())
        //{
        destArchetype = GetOrCreateArchetype_Add(destArchetype, ComponentTypeId<EcsName>::Id());
        //}

        for(uint32_t idx = 0 ; idx < desc.add.count; ++idx)
        {
            destArchetype = GetOrCreateArchetype_Add(destArchetype, desc.add[idx]);
        }

        if(destArchetype)
        {
            if(destArchetype->count == destArchetype->capacity)
            {
                GrowArchetype(*destArchetype);
            }

            for(uint32_t i = 0; i < destArchetype->componentSet.count; i++)
            {
                //skip no data tag and pair
                int32_t destColIdx = destArchetype->componentMap[i];

                if(destColIdx == -1)
                {
                    continue;
                }

                Column& destCol = destArchetype->columns[destColIdx];
                TypeInfo& ti = *destCol.typeInfo;

                void* dest = OFFSET(destCol.data, ti.size * destArchetype->count);

                if(destArchetype->componentSet[i] == ComponentTypeId<EcsName>::Id())
                {
                    EcsName ecsName;
                    ecsName.name = desc.name;
                    ti.hook.moveCtor(dest, &ecsName);
                }
                else
                {
                    void* data = desc.componentData[destArchetype->componentSet[i]];
                    
                    if(ti.hook.moveCtor)
                    {
                        ti.hook.moveCtor(dest, data);
                    }
                    else if(ti.hook.copyCtor)
                    {
                        ti.hook.copyCtor(dest, data);
                    }
                    else
                    {
                        std::memcpy(dest, data, ti.size);
                    }
                }
            }

            destArchetype->entities[destArchetype->count] = desc.id;
            r.archetype = destArchetype;
            r.row = destArchetype->count;
            ++destArchetype->count;
        }

        desc.add.Destroy(m_wAllocator);
        desc.componentData.Destroy();
    }

    void World::AddComponent(EntityId eId, EntityId cId)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);
        TypeInfo* cTi = m_typeInfos[cId];
        
        assert(cTi->IsComponent());
        assert(r);
        assert(!(r->archetype->componentSet.Has(cId)));
        

        Archetype* destArchetype = GetOrCreateArchetype_Add(r->archetype, cId);

        MoveArchetype_Add(eId, *r, destArchetype);

        cTi->hook.onAdd();
    }

    void World::AddRelationship(EntityId eId, EntityId relationId, EntityId targetId)
    {
        EntityId relationshipId = MakeRelationship(relationId, targetId);
        TypeInfo* ti = m_typeInfos.GetValue(relationId);
         
        assert(ti->IsRelation()); 

        if(!m_typeInfos.ContainsKey(relationshipId))
        {
            TypeInfoBuilder tiBuilder(this);
            tiBuilder.Relationship(relationId, targetId).Register();
        }

        TypeInfo* relationshipTi = m_typeInfos[relationshipId];
        
        EntityRecord* r = m_entityIndex.GetPageData(eId);

        assert(r);
        assert(!r->archetype->componentSet.Has(relationshipId));

        if(ti->IsExclusive())
        {
            if(r->archetype->componentSet.HasRelationship(relationId))
            {
                return;
            }
        }

        Archetype* destArchetype = GetOrCreateArchetype_Add(r->archetype, relationshipId);

        MoveArchetype_Add(eId, *r, destArchetype);

        relationshipTi->hook.onAdd();
    }

    void World::AddTag(EntityId eId, EntityId cId)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);
        TypeInfo* ti = m_typeInfos[cId];
        
        assert(ti->IsTag());
        assert(r);
        assert(!r->archetype->componentSet.Has(cId));

        Archetype* destArchetype = GetOrCreateArchetype_Add(r->archetype, cId);

        MoveArchetype_Add(eId, *r, destArchetype);

        ti->hook.onAdd();
    }

    void World::RemoveComponent(EntityId eId, EntityId cId)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);

        assert(r);
        assert(r->archetype);
        Archetype* srcArchetype = r->archetype;
        assert(r->archetype->componentSet.Has(cId));

        Archetype* destArchetype = GetOrCreateArchetype_Remove(srcArchetype, cId);

        MoveArchetype_Remove(eId, *r, destArchetype);

        TypeInfo* ti = m_typeInfos[cId];
        assert(ti);
        ti->hook.onRemove();
    }

    void World::Set(EntityId eId, EntityId cId, void* data)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);
        assert(r);
        assert(r->archetype);
        assert(r->dense);

        int32_t idx = r->archetype->componentSet.Search(cId);
        assert(idx != -1);
        
        int32_t colIdx = r->archetype->componentMap[idx];
        assert(colIdx != -1);

        Column& col = r->archetype->columns[colIdx];
        TypeInfo* ti = col.typeInfo;
        
        assert(ti);

        void* dest = OFFSET_ELEMENT(col.data, ti->size ,r->row);

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

        ti->hook.onSet(dest);
    }

    void World::Set(EntityId eId, EntityId cId, const void* data)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);
        assert(r);
        assert(r->archetype);
        assert(r->dense);

        int32_t idx = r->archetype->componentSet.Search(cId);
        assert(idx != -1);
        
        int32_t colIdx = r->archetype->componentMap[idx];
        assert(colIdx != -1);

        Column& col = r->archetype->columns[colIdx];
        TypeInfo* ti = col.typeInfo;
        
        assert(ti);

        void* dest = OFFSET_ELEMENT(col.data, ti->size ,r->row);

        if (ti->hook.copyCtor)
        {
            ti->hook.copyCtor(dest, data);
        }
        else
        {
            std::memcpy(dest, data, ti->size);
        }

        ti->hook.onSet(dest);
    }

    void* World::Get(EntityId eId, EntityId cId)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);
        assert(r);
        assert(r->archetype);
        assert(r->dense);

        int32_t idx = r->archetype->componentSet.Search(cId);
        assert(idx != -1);
        int32_t colIdx = r->archetype->componentMap[idx];
        assert(colIdx != -1);

        Column& col = r->archetype->columns[colIdx];
        TypeInfo& ti = *col.typeInfo;

        void* component = OFFSET_ELEMENT(col.data, ti.size ,r->row);

        return component;    
    }
    
    void World::GrowArchetype(Archetype& archetype)
    {
        uint32_t newCapacity = archetype.capacity * 2;

        //grow entities
        EntityId* newEntities =
            PTR_CAST(m_wAllocator.Init(sizeof(EntityId) * newCapacity), EntityId);

        std::memcpy(newEntities, archetype.entities, sizeof(EntityId) * archetype.capacity);
        m_wAllocator.Free(sizeof(EntityId) * archetype.capacity, archetype.entities);

        archetype.entities = newEntities;

        //grow column data
        for(uint32_t idx = 0; idx < archetype.columnCount; idx++)
        {
            Column& col = archetype.columns[idx];
            TypeInfo* ti = col.typeInfo;
            void* newColData = m_wAllocator.Init(ti->size * newCapacity);

            if(ti->hook.moveCtor || ti->hook.copyCtor)
            {
                for(uint32_t row = 0; row < archetype.count; row++)
                {
                    void* dest = OFFSET(newColData, ti->size * row);
                    void* src = OFFSET(col.data, ti->size * row);

                    if(ti->hook.moveCtor)
                    {
                        ti->hook.moveCtor(dest, src);
                    }
                    else
                    {
                        ti->hook.copyCtor(dest, src);
                    }

                    if(ti->hook.dtor)
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

    void World::SwapBack(EntityRecord& r)
    {
        assert(r.archetype);
        assert(r.dense);

        Archetype& archetype = *r.archetype;

        if(archetype.count == 0 || (archetype.count == r.row + 1))
        {
            return;
        }

        EntityId swapId = archetype.entities[r.row];
        EntityId backId = archetype.entities[archetype.count - 1];

        EntityRecord* backRecord = m_entityIndex.GetPageData(backId);
        assert(backRecord);

        archetype.entities[r.row] = backId;
        archetype.entities[archetype.count - 1] = swapId;

        for(uint32_t i = 0; i < r.archetype->componentSet.count; i++)
        {
            Column& col = r.archetype->columns[i];
            TypeInfo& ti = *col.typeInfo;
            void* swap = OFFSET(col.data, ti.size * r.row);
            void* back = OFFSET(col.data, ti.size * (archetype.count - 1));

            void* temp = m_wAllocator.Init(ti.size);

            if(ti.hook.moveCtor)
            {
                ti.hook.moveCtor(temp, swap);
                ti.hook.moveCtor(swap, back);
                ti.hook.moveCtor(back, temp);
            }
            else if(ti.hook.copyCtor)
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

            if(ti.hook.dtor)
            {
                ti.hook.dtor(temp);
            }

            m_wAllocator.Free(ti.size, temp);
        }

        backRecord->row = r.row;
        r.row = archetype.count - 1;
    }

    Archetype* World::CreateArchetype(ComponentSet&& componentSet)
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

        archetype.columns =
            PTR_CAST(m_wAllocator.Init(sizeof(Column) * componentSet.count), Column);
        archetype.entities =
            PTR_CAST(m_wAllocator.Init(sizeof(EntityId) * DefaultArchetypeCapacity), EntityId);
        archetype.componentMap =
            PTR_CAST(m_wAllocator.Calloc(sizeof(int32_t) * componentSet.count * 2), int32_t);

        //Set up component map to determine what column has data
        uint32_t dataColCounter = 0;
        for(uint32_t idx = 0; idx < componentSet.count; idx++)
        {
            TypeInfo* ti = m_typeInfos[LO_ENTITY_ID(componentSet[idx])];
            if(ti->HasData())
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
        Archetype* rArchetype = m_archetypes.GetPageData(id);

        //Add newly created archetype to component record for query purpose
        //if component record is relationship,
        //add this archetype to base relation and specific relationship
        
        //NOTE: Test this
        for(uint32_t idx = 0; idx < componentSet.count; idx++)
        {
            ComponentRecord& cr = m_componentIndex[componentSet[idx]];

            //union pair
            if(cr.typeInfo->IsRelationship())
            {
                ComponentRecord& pCr = m_componentIndex[LO_ENTITY_ID(componentSet[idx])];

                if(pCr.archetypeStore.count == pCr.archetypeStore.capacity)
                {
                    pCr.archetypeStore.Grow(m_wAllocator);
                }

                pCr.archetypeStore.store[pCr.archetypeStore.count] = rArchetype;
                ++pCr.archetypeStore.count;
            }

            if(cr.archetypeStore.count == cr.archetypeStore.capacity)
            {
                cr.archetypeStore.Grow(m_wAllocator);
            }

            cr.archetypeStore.store[cr.archetypeStore.count] = rArchetype;
            ++cr.archetypeStore.count;
        }

        m_mappedArchetype.Insert(std::move(componentSet), rArchetype);

        return rArchetype;
    }

    Archetype* World::GetArchetype(const ComponentSet& componentSet)
    {
        if(m_mappedArchetype.ContainsKey(componentSet))
        {
            return m_mappedArchetype[componentSet];
        }

        return nullptr;
    }

    Archetype* World::GetOrCreateArchetype_Add(Archetype* src, EntityId cId)
    {
        uint32_t srcCount = 0;
        if(src)
        {
            srcCount = src->componentSet.count;
        }

        Archetype* dest = nullptr;

        //find or create edge
        if(src)
        {
            if(src->addEdges.ContainsKey(cId))
            {
                dest = src->addEdges[cId];
            }
            else
            {
                //CREATE EDEGE
                uint32_t count = srcCount + 1;

                ComponentSet cs;
                cs.Init(m_wAllocator, count);
                std::memcpy(cs.idArr, src->componentSet.idArr, srcCount * sizeof(EntityId));
                cs[count - 1] = cId;
                cs.Sort();

                dest = GetArchetype(cs);

                if(!dest)
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

            if(!dest)
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

    Archetype* World::GetOrCreateArchetype_Remove(Archetype* src, EntityId cId)
    {
        assert(src);

        uint32_t srcCount = 0;
        srcCount = src->componentSet.count;

        Archetype* dest = nullptr;

        if(src->removeEdges.ContainsKey(cId))
        {
            dest = src->removeEdges[cId];
        }
        else
        {
            //CREATE EDEGE
            uint32_t count = srcCount - 1;
            
            assert(count >= 0);
            if(count == 0)
            {
                return nullptr;
            }
            else
            {
                ComponentSet cs;
                cs.Init(m_wAllocator, count);

                //TODO: Review this later
                int32_t rIdx = src->componentSet.Search(cId);
                assert(rIdx != -1);

                std::memcpy(cs.idArr, src->componentSet.idArr, rIdx * sizeof(EntityId));
                std::memcpy(cs.idArr + rIdx, src->componentSet.idArr + rIdx + 1, (count - rIdx - 1) * sizeof(EntityId));
                cs.Sort();

                dest = GetArchetype(cs);

                if(!dest)
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

    void World::MoveArchetype_Add(EntityId eId, EntityRecord& r, Archetype* destArchetype)
    {
        assert(destArchetype);

        if(destArchetype->count == destArchetype->capacity)
        {
            GrowArchetype(*destArchetype);
        }

        //empty entity
        if(!r.archetype)
        {
            if(destArchetype->columnCount == 1)
            {
                TypeInfo& ti = *destArchetype->columns[0].typeInfo;
                void* dataAddr = OFFSET(destArchetype->columns[0].data, destArchetype->count * ti.size);
                ti.hook.ctor(dataAddr);
            }
        }
        //at least 1 component
        else
        {
            //SWAP BACK IN SRC ARCHETYPE
            SwapBack(r);

            for(uint32_t i = 0; i < destArchetype->componentSet.count; i++)
            {
                //skip no data tag and relationship
                int32_t destColIdx = destArchetype->componentMap[i];

                if(destColIdx == -1)
                {
                    continue;
                }

                Column& destCol = destArchetype->columns[destColIdx];
                TypeInfo& ti = *destCol.typeInfo;

                void* dest = OFFSET(destCol.data, ti.size * destArchetype->count);

                int32_t srcIndex = r.archetype->componentSet.Search(destArchetype->componentSet[i]);

                if(srcIndex == -1)
                {
                    //find the newly added component
                    ti.hook.ctor(dest);
                }
                else
                {
                    int32_t srcColIdx = r.archetype->componentMap[srcIndex];

                    if(srcColIdx == -1)
                    {
                        assert(0 && "Mismatch type");
                    }

                    Column& srcCol = r.archetype->columns[srcColIdx];
                    void* src = OFFSET(srcCol.data, ti.size * r.row);

                    if(ti.hook.moveCtor)
                    {
                        ti.hook.moveCtor(dest, src);
                    }
                    else if(ti.hook.copyCtor)
                    {
                        ti.hook.copyCtor(dest, src);
                    }
                    else
                    {
                        std::memcpy(dest, src, ti.size);
                    }

                    if(ti.hook.dtor)
                    {
                        ti.hook.dtor(src);
                    }
                }
            }

            --r.archetype->count;
        }

        destArchetype->entities[destArchetype->count] = eId;
        r.archetype = destArchetype;
        r.row = destArchetype->count;
        ++destArchetype->count;
    }

    void World::MoveArchetype_Remove(EntityId eId, EntityRecord& r, Archetype* destArchetype)
    {
        Archetype* srcArchetype = r.archetype;
        SwapBack(r);

        if(!destArchetype)
        {
            if(srcArchetype->columnCount == 1 && srcArchetype->componentSet.count == 1)
            {
                int32_t srcColIdx = srcArchetype->componentMap[0];
                Column& srcCol = srcArchetype->columns[srcColIdx];
                TypeInfo& ti = *srcCol.typeInfo;

                if(ti.HasData() && ti.hook.dtor)
                {
                    void* src = OFFSET_ELEMENT(srcCol.data, ti.size, srcArchetype->count - 1);
                    ti.hook.dtor(src);
                }

                r.row = 0;
                --r.archetype->count;
                r.archetype = destArchetype;
            }
            else
            {
                assert(0);
            }
        }
        else
        {
            if(destArchetype->count == destArchetype->capacity)
            {
                GrowArchetype(*destArchetype);
            }

            for(uint32_t idx = 0; idx < srcArchetype->componentSet.count; idx++)
            {
                int32_t srcColIdx = srcArchetype->componentMap[idx];

                if(srcColIdx == -1)
                {
                    continue;
                }

                Column& srcCol = srcArchetype->columns[srcColIdx];
                TypeInfo& ti = *srcCol.typeInfo;
                void* src = OFFSET(srcCol.data, ti.size * r.row);


                int32_t destIdx = destArchetype->componentSet.Search(srcArchetype->componentSet[idx]);
                if(destIdx != -1)
                {
                    int32_t destColIdx = destArchetype->componentMap[destIdx];

                    assert(destColIdx != -1);

                    Column& destCol = destArchetype->columns[destColIdx];
                    void* dest = OFFSET(destCol.data, ti.size * destArchetype->count);

                    if(ti.hook.moveCtor)
                    {
                        ti.hook.moveCtor(dest, src);
                    }
                    else if(ti.hook.copyCtor)
                    {
                        ti.hook.copyCtor(dest, src);
                    }
                    else
                    {
                        std::memcpy(dest, src, ti.size);
                    }
                }

                if(ti.hook.dtor)
                {
                    ti.hook.dtor(src);
                }
            }

            destArchetype->entities[destArchetype->count] = eId;
            --r.archetype->count;
            r.archetype = destArchetype;
            r.row = destArchetype->count;
            ++destArchetype->count;
        }


    }
    
    void World::Progress(double dt)
    {
        if(m_isDefered == false)
        {
            m_isDefered = true;

            for(uint32_t idx = 0; idx < m_systemStore.count; idx++)
            {
                SystemCallback& sc = m_systemStore.store[idx];

                ArchetypeLinkedList* head = sc.archetypeList;

                void** componentsData = PTR_CAST(m_wAllocator.Init(sizeof(void*) * sc.componentSet.count), void*);

                while(head->archetype)
                {
                    Archetype* archetype = head->archetype;

                    for(uint32_t row = 0; row < archetype->count; row++)
                    {
                        QueryIterator it;
                        it.archetype = archetype;
                        it.world = this;
                        it.row = row;

                        for(uint32_t idx = 0; idx < sc.componentSet.count; idx++)
                        {
                            int32_t cIdx = archetype->componentSet.Search(sc.componentSet[idx]);

                            if(cIdx == -1)
                            {
                                cIdx = archetype->componentSet.SearchPair(sc.componentSet[idx]);
                            }

                            assert(cIdx != -1);

                            int32_t colIdx = archetype->componentMap[cIdx];

                            if(colIdx == -1)
                            {
                                componentsData[idx] = nullptr;
                            }
                            else
                            {
                                Column& col = archetype->columns[colIdx];
                                TypeInfo& ti = *col.typeInfo;
                                void* comData = OFFSET(col.data, ti.size * row);
                                componentsData[idx] = comData;
                            }
                        }

                        //EXECUTE
                        sc.Execute(&it, componentsData);
                    }

                    head = head->next;
                }

                m_wAllocator.Free(sizeof(void*) * sc.componentSet.count, componentsData);
            }

            m_isDefered = false;
        }
    }

    void World::Destroy()
    {
        //clear archetype
        for(uint32_t aIdx = 1; aIdx <= m_archetypes.GetCount(); aIdx++)
        {
            Archetype* archetype = m_archetypes.GetPageData(m_archetypes.GetId(aIdx));
            assert(archetype);

            for(uint32_t cIdx = 0; cIdx < archetype->columnCount; cIdx++)
            {
                Column& col = archetype->columns[cIdx];
                TypeInfo& ti = *col.typeInfo;

                if(ti.hook.dtor)
                {
                    for(uint32_t row = 0; row < archetype->count; row++)
                    {
                        void* component = OFFSET(col.data, (ti.size * row));

                        ti.hook.dtor(component);
                    }
                }

                m_wAllocator.Free(ti.size * archetype->capacity, col.data);
            }

            m_wAllocator.Free(sizeof(EntityId) * archetype->capacity, archetype->entities);
            m_wAllocator.Free(sizeof(int32_t) * archetype->componentSet.count * 2, archetype->componentMap);
            m_wAllocator.Free(sizeof(Column) * archetype->componentSet.count, archetype->columns);
            archetype->addEdges.Destroy();
            archetype->removeEdges.Destroy();
        }



        m_archetypes.Destroy();
        m_entityIndex.Destroy();

        //NOTE: should clear the data if keeping metadata between world is favorable 
        m_componentIndex.Destroy();

        m_mappedArchetype.Destroy();
        m_typeInfos.Destroy();

        m_allocators.archetypes.Destroy();
        m_componentStore.Destroy(m_wAllocator);

        for(uint32_t sIdx = 0; sIdx < m_systemStore.count; sIdx++)
        {
            SystemCallback& sc = m_systemStore.store[sIdx];
            ArchetypeLinkedList* head = sc.archetypeList;

            while(head->archetype)
            {
                ArchetypeLinkedList* freeNode = head;
                head = head->next;

                ArchetypeLinkedList::Free(m_wAllocator, freeNode);
            }

            sc.componentSet.Destroy(m_wAllocator);
        }
        m_systemStore.Destroy(m_wAllocator);

        for(uint32_t bIdx = 1; bIdx <= m_wAllocator.m_sparse.GetCount(); bIdx++)
        {
            BlockAllocator* ba = m_wAllocator.m_sparse.GetPageData(m_wAllocator.m_sparse.GetId(bIdx));
            assert(ba);

            ba->Destroy();
        }

        m_wAllocator.m_sparse.Destroy();
        m_wAllocator.m_chunks.Destroy();
    }

}
