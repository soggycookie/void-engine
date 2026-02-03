#pragma once
#include "ecs_pch.h"
#include "ds/world_allocator.h"
#include "ds/hash_map.h"
#include "entity.h"
#include "ecs_type.h"

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
            : m_nextFreeId(0), m_isDefered(false)
        {
        }

        void Init();

        void InitAllocators();

        Entity CreateEntity();

        Entity CreateEntity(const char* name);

        Entity CreateEntity(LoEntityId id);
        Entity CreateEntity(LoEntityId id, const char* name);

        EntityRecord* GetEntityRecord(EntityId id);
        Entity GetEntity(EntityId id);


        //NOTE: Add type, event hook
        template<typename Component>
        TypeInfoBuilder<Component> RegisterComponent()
        {
            TypeInfo* ti = new (m_wAllocator.Alloc(sizeof(TypeInfo))) TypeInfo();
            ti->size = sizeof(Component);
            ti->alignment = alignof(Component);
            ti->id = GetComponentId<Component>();
            ti->entityId = 0;
            ti->name = typeid(Component).name();
            
            auto e = CreateEntity(ti->name);
            ti->entityId = e.GetFullId();

            assert(std::is_destructible_v<Component>);
            assert(std::is_trivially_constructible_v<Component>);

            TypeInfoBuilder<Component> tiBuilder {*ti};

            tiBuilder.Ctor(
                [](void* dest)
                {
                    new (dest) Component();
                }
            );

            if constexpr(std::is_move_constructible_v<Component>)
            {
                tiBuilder.MoveCtor(
                    [](void* dest, void* src)
                    {
                        new (dest) Component(std::move(*PTR_CAST(src, Component)));
                    }                
                );
            }
            else if constexpr (!std::is_trivially_constructible_v<Component>)
            {
                tiBuilder.CopyCtor(
                    [](void* dest, const void* src)
                    {
                        new (dest) Component(*PTR_CAST(src, Component));
                    }
                );
            }
            
            if constexpr (!std::is_trivially_destructible_v<Component>)
            {
                tiBuilder.Dtor(
                    [](void* src)
                    {
                        Component* c = PTR_CAST(src, Component);
                        c->~Component();
                    }
                );
            }


            tiBuilder.AddEvent(
                []()
                {
                    std::cout << "Add component " << ComponentName<Component>::name << std::endl;
                }
            );
            
            tiBuilder.RemoveEvent(
                []()
                {
                    std::cout << "Remove component" << ComponentName<Component>::name << std::endl;
                }
            );
            
            tiBuilder.SetEvent(
                [](void* dest)
                {
                    std::cout << "Set component" << ComponentName<Component>::name << std::endl;
                }
            );

            m_typeInfos.Insert(ti->id, ti);
            return tiBuilder;        
        }

        template<typename Component>
        void AddComponent(EntityId id)
        {
            EntityRecord* r = m_entityIndex.GetPageData(id);

            assert(r);
            
            if(r->archetype)
            {
                int32_t s = r->archetype->components.Search(GetComponentId<Component>());
                
                if(s != -1)
                {
                    return;
                }
            }

            uint32_t count = 1;
            uint32_t* arr = PTR_CAST(m_wAllocator.Alloc(sizeof(ComponentId) * count), ComponentId);

            ComponentDiff cdiff;
            cdiff.ids = arr;
            cdiff.count = count;
            cdiff.ids[0] = GetComponentId<Component>();

            Archetype* destArchetype = GetOrCreateAddedArchetype(r->archetype, std::move(cdiff));

            if(destArchetype->count == destArchetype->capacity)
            {
                GrowArchetype(*destArchetype);
            }

            //empty entity
            if(!r->archetype)
            {
                for(uint32_t i = 0; i < destArchetype->components.count; i++)
                {
                    TypeInfo& ti = *(destArchetype->columns[i].typeInfo);
                    void* dataAddr = OFFSET(destArchetype->columns[i].data, destArchetype->count * ti.size);
                    ti.hook.ctor(dataAddr);
                }
            }
            //at least 1 component
            else
            {
                //SWAP BACK IN SRC ARCHETYPE
                SwapBack(*r);

                for(uint32_t i = 0; i < destArchetype->components.count; i++)
                {
                    Column& destCol = destArchetype->columns[i];
                    TypeInfo& ti = *destCol.typeInfo;
                    void* dataAddr = OFFSET(destCol.data, count * ti.size);
                    
                    int32_t srcIndex = r->archetype->components.Search(destArchetype->components.ids[i]);
                    
                    if(srcIndex == -1)
                    {
                        ti.hook.ctor(dataAddr);
                    }
                    else
                    {
                        Column& srcCol = r->archetype->columns[srcIndex];

                        void* dest = OFFSET(destCol.data, ti.size * destArchetype->count);
                        void* src = OFFSET(srcCol.data, ti.size * r->row);

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

            }

            destArchetype->entities[destArchetype->count] = id;
            r->archetype = destArchetype;
            r->row = destArchetype->count;
            ++destArchetype->count;

            TypeInfo& ti = *m_typeInfos[GetComponentId<Component>()];
            ti.hook.onAdd();
        }
        
        void GrowArchetype(Archetype& archetype)
        {
            uint32_t newCapacity = archetype.capacity * 2;

            //grow entities
            EntityId* newEntities = 
                PTR_CAST(m_wAllocator.Alloc(sizeof(EntityId) * DefaultArchetypeCapacity), EntityId);

            std::memcpy(newEntities, archetype.entities, sizeof(EntityId) * archetype.count);
            m_wAllocator.Free(sizeof(EntityId) * archetype.capacity, archetype.entities);
            
            //grow column data
            for(uint32_t i = 0; i < archetype.components.count; i++)
            {
                Column& col = archetype.columns[i]; 
                auto ti = col.typeInfo;
                void* newColData = m_wAllocator.Alloc(ti->size * newCapacity);

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

                        ti->hook.dtor(src);
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

        void SwapBack(EntityRecord& r)
        {
            assert(r.archetype);
            assert(r.dense);

            Archetype& archetype = *r.archetype;

            if(archetype.count == r.row + 1)
            {
                return;
            }

            EntityId swapId = archetype.entities[r.row];
            EntityId backId = archetype.entities[archetype.count - 1];
            
            EntityRecord* backRecord = m_entityIndex.GetPageData(backId);
            assert(backRecord);

            archetype.entities[r.row] = backId;
            archetype.entities[archetype.count - 1] = swapId;

            for(uint32_t i = 0; i < r.archetype->components.count; i++)
            {
                Column& col = r.archetype->columns[i];
                TypeInfo& ti = *col.typeInfo;
                void* swap = OFFSET(col.data, ti.size * r.row);
                void* back = OFFSET(col.data, ti.size * (archetype.count - 1));

                void* temp = m_wAllocator.Alloc(ti.size);
                
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

        Archetype* CreateArchetype(ComponentSet&& componentSet)
        {
            ArchetypeId id = GetArchetypeId();
            Archetype archetype;
            archetype.id = id;
            archetype.count = 0;
            archetype.capacity = DefaultArchetypeCapacity;
            archetype.components = componentSet;
            archetype.addedEdges.Init(&m_wAllocator, DefaultArchetypeCapacity);
            archetype.removedEdges.Init(&m_wAllocator, DefaultArchetypeCapacity);
            
            archetype.columns = 
                PTR_CAST(m_wAllocator.Alloc(sizeof(Column) * componentSet.count), Column);
            archetype.entities = 
                PTR_CAST(m_wAllocator.Alloc(sizeof(EntityId) * DefaultArchetypeCapacity), EntityId);

            for(uint32_t i = 0; i < componentSet.count; i++)
            {
                auto ti = m_typeInfos[componentSet.ids[i]];
                archetype.columns[i].typeInfo = ti;
                archetype.columns[i].data = 
                    m_wAllocator.Alloc(ti->size * DefaultArchetypeCapacity);
            }            

            m_archetypes.PushBack(id, std::move(archetype));
            
            auto ra = m_archetypes.GetPageData(id);

            m_mappedArchetype.Insert(componentSet, ra);

            return ra;
        }

        Archetype* GetArchetype(const ComponentSet& componentSet)
        {
            if(m_mappedArchetype.ContainsKey(componentSet))
            {
                return m_mappedArchetype[componentSet];
            }

            return nullptr;
        }

        Archetype* GetOrCreateAddedArchetype(Archetype* src, ComponentDiff&& componentDiff)
        {
            uint32_t srcCount = 0;
            if(src)
            {
                srcCount = src->components.count;
            }

            Archetype* dest = nullptr;

            //find or create edge
            if(src)
            {
                if(src->addedEdges.ContainsKey(componentDiff))
                {
                    dest = src->addedEdges[componentDiff];
                    m_wAllocator.Free(sizeof(ComponentId) * componentDiff.count, componentDiff.ids);
                }
                else
                {
                    //CREATE EDEGE
                    uint32_t count = srcCount + componentDiff.count;
                    uint32_t* arr = PTR_CAST(m_wAllocator.Alloc(sizeof(ComponentId) * count), ComponentId);
                    
                    ComponentSet cs;
                    cs.ids = arr;
                    cs.count = count;
                    std::memcpy(cs.ids, src->components.ids, srcCount * sizeof(ComponentId));
                    std::memcpy((cs.ids + srcCount), componentDiff.ids, componentDiff.count * sizeof(ComponentId));
                    cs.Sort();

                    dest = GetArchetype(cs);

                    if(!dest)
                    {
                        dest = CreateArchetype(std::move(cs));
                    }
                    else
                    {
                        m_wAllocator.Free(sizeof(ComponentId) * cs.count, cs.ids);
                    }

                    src->addedEdges.Insert(std::move(componentDiff), dest);
                }
            }
            else
            {
                componentDiff.Sort();

                dest = GetArchetype(componentDiff);

                if(!dest)
                {
                    dest = CreateArchetype(std::move(componentDiff));
                }
                else
                {
                    m_wAllocator.Free(sizeof(ComponentId) * componentDiff.count, componentDiff.ids);
                }
            }

            assert(dest);

            return dest;
        }

        template<typename Component>
        void Set(EntityId id, Component&& c)
        {
            EntityRecord* r = m_entityIndex.GetPageData(id);
            assert(r);
            assert(r->archetype);
            assert(r->dense);

            int32_t idx = r->archetype->components.Search(GetComponentId<Component>());
        
            assert(idx != -1);

            Component& component = *CAST_OFFSET_ELEMENT(r->archetype->columns[idx].data, Component, sizeof(Component), r->row);

            component = std::move(c);
        }

        template<typename Component>
        Component& Get(EntityId id)
        {
            EntityRecord* r = m_entityIndex.GetPageData(id);
            assert(r);
            assert(r->archetype);
            assert(r->dense);

            int32_t idx = r->archetype->components.Search(GetComponentId<Component>());
        
            assert(idx != -1);

            Component& component = *CAST_OFFSET_ELEMENT(r->archetype->columns[idx].data, Component, sizeof(Component), r->row);
            
            return component;
        }

        void Destroy()
        {
            //clear archetype
            for(uint32_t aIdx = 1; aIdx <= m_archetypes.GetCount(); aIdx++)
            {
                Archetype* archetype = m_archetypes.GetPageData(m_archetypes.GetId(aIdx));
                assert(archetype);

                for(uint32_t cIdx = 0; cIdx < archetype->components.count; cIdx++)
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
                m_wAllocator.Free(sizeof(Column) * archetype->components.count, archetype->columns);
                archetype->addedEdges.Destroy();
                archetype->removedEdges.Destroy();
            }



            m_archetypes.Destroy();
            m_entityIndex.Destroy();
            m_componentRecords.Destroy();
            m_mappedArchetype.Destroy();
            m_typeInfos.Destroy();

            m_allocators.archetypes.Destroy();

            for(uint32_t bIdx = 1; bIdx <= m_wAllocator.m_sparse.GetCount(); bIdx++)
            {
                BlockAllocator* ba = m_wAllocator.m_sparse.GetPageData(m_wAllocator.m_sparse.GetId(bIdx));
                assert(ba);

                ba->Destroy();
            }

            m_wAllocator.m_sparse.Destroy();
            m_wAllocator.m_chunks.Destroy();
        }

    public:
        WorldAllocator m_wAllocator;        
        Allocators m_allocators;        
        SparseSet<EntityRecord> m_entityIndex;
        SparseSet<Archetype> m_archetypes;
        HashMap<ComponentId, ComponentRecord> m_componentRecords;
        HashMap<ComponentId, TypeInfo*> m_typeInfos;
        HashMap<ComponentSet, Archetype*> m_mappedArchetype; //value hold a ref to key, does not change the value's key ref
        uint32_t m_nextFreeId;
        bool m_isDefered;
    };
}

#include "entity_builder.inl"