#pragma once
#include "entity.h"
#include "internal_component.h"

namespace ECS
{
    struct ArchetypeLinkedList
    {
        Archetype* archetype;
        ArchetypeLinkedList* next;

        static ArchetypeLinkedList* Init(WorldAllocator& wAllocator)
        {
            ArchetypeLinkedList* all = PTR_CAST(wAllocator.Calloc(sizeof(ArchetypeLinkedList)), ArchetypeLinkedList);
        
            return all;
        }

        static void Free(WorldAllocator& wAllocator, void* addr)
        {
            wAllocator.Free(sizeof(ArchetypeLinkedList), addr); 
        }
    };

    class World;

    struct QueryIterator
    {
        World* world;
        Archetype* archetype;
        uint32_t row;
        //double deltaTime;

        Entity GetEntity()
        {
            EntityId id = archetype->entities[row];

            return Entity(id, world);
        }

        template<typename Component>
        Component& Get()
        {
            uint32_t colIdx = archetype->componentSet.Search(ComponentTypeId<EntityId>::id);

            assert(colIdx != -1);

            Column& col = archetype->columns[colIdx];
            TypeInfo& ti = *col.typeInfo;
            void* comData = OFFSET(col.data, ti.size * row);
            
            return *PTR_CAST(comData, Component);
        }
    };

    enum TraverseMethod : uint16_t
    {
        SELF,
        UP,
        SELF_UP,
        CASCADE
    };

    enum TermOp : uint16_t
    {
        HAS,
        NOT,
        OPTIONAL
    };

    enum TermBehavior : uint16_t
    {
        READ_WRITE,
        STRUCTURE_CHANGE
    };

    struct QueryTerm
    {
        QueryTerm(): 
            id(0), first(0), second(0), 
            travTarget(0), trav(SELF), op(HAS), 
            behavior(READ_WRITE), fieldId(0)
        {
        }

        EntityId id;
        EntityId first;
        EntityId second;
        EntityId travTarget;
        TraverseMethod trav;
        TermOp op;
        TermBehavior behavior;
        uint16_t fieldId;
    };


    struct QueryDesc
    {
        QueryDesc():
            id(0), cache(false)
        {
        }

        QueryTerm terms[32];
        EntityId id;
        bool cache;
    };

    struct QueryCache
    {
    };

    struct Query
    {
        EntityId id;
        QueryTerm* terms;
        QueryCache* cache;
        uint32_t termCount;
    };


    template<typename... Components>
    class QueryBuilder
    {
    public:
        QueryBuilder(World* world)
            : m_world(world), m_currTermIdx(0), m_desc(), m_firstTerm(true)
        {
            assert(m_world);
        }

        template<typename T>
        QueryBuilder<Components...>& Term(EntityId id)
        {
            if(!m_firstTerm)
            {
                m_desc.terms[m_currTermIdx++] = m_currTerm;
                m_currTerm = QueryTerm();
                m_currTerm.fieldId = m_currTermIdx;
            }
            else
            {
                m_firstTerm = false;
            }
                        
            m_currTerm.id = ComponentTypeId<T>::id; 

            return *this;
        }

        QueryBuilder<Components...>& Term(EntityId first, EntityId second = EcsAnyId)
        {
            if(!m_firstTerm)
            {
                m_desc.terms[m_currTermIdx++] = m_currTerm;
                m_currTerm = QueryTerm();
                m_currTerm.fieldId = m_currTermIdx;
            }            
            else
            {
                m_firstTerm = false;
            }
            
            m_currTerm.first = first;
            m_currTerm.second = second;
            m_currTerm.id = MakeRelationship(first, second);

            return *this;
        }

        template<typename T>
        QueryBuilder<Components...>& Term()
        {
            return Term(ComponentTypeId<T>::id);
        }

        QueryBuilder<Components...>& Traverse(TraverseMethod method)
        {
            m_currTerm.trav = method;
            return *this;
        }

        QueryBuilder<Components...>& TraveseTarget(EntityId targetId)
        {
            m_currTerm.travTarget = targetId;
            return *this;
        }

        QueryBuilder<Components...>& TraveseTarget(EntityId first, EntityId second = EcsAnyId)
        {
            m_currTerm.travTarget = MakeRelationship(first, second);
            return *this;
        }

        QueryBuilder<Components...>& Op(TermOp op)
        {
            m_currTerm.op = op;
            return *this;
        }

        QueryBuilder<Components...>& Cache(EntityId cacheId)
        {
            m_desc.cache = true;
            m_desc.id = cacheId;
            return *this;
        }       

        QueryBuilder<Components...>& Scope()
        {
            m_desc.cache = false;

            return *this;
        }

        template<typename T>
        QueryBuilder<Components...>& With()
        {
            return Term<T>().With();
        }
        
        template<typename T>
        QueryBuilder<Components...>& Without()
        {
            return Term<T>().Without();
        }

        template<typename T>
        QueryBuilder<Components...>& Optional()
        {
            return Term<T>().Optional();
        }

        QueryBuilder<Components...>& With()
        {
            return Op(HAS);
        }
        
        QueryBuilder<Components...>& Without()
        {
            return Op(NOT);
        }

        QueryBuilder<Components...>& Optional()
        {
            return Op(OPTIONAL);
        }

        QueryBuilder<Components...>& SelfUp(EntityId target)
        {
            return Traverse(SELF_UP).TraveseTarget(target);
        }

        QueryBuilder<Components...>& SelfUp(EntityId first, EntityId second)
        {
            return Traverse(SELF_UP).TraveseTarget(first, second);
        }
        
        QueryBuilder<Components...>& Up(EntityId target)
        {
            return Traverse(UP).TraveseTarget(target);
        }

        QueryBuilder<Components...>& Up(EntityId first, EntityId second)
        {
            return Traverse(SELF_UP).TraveseTarget(first, second);
        }

        QueryBuilder<Components...>& Cascade(EntityId target)
        {
            return Traverse(CASCADE).TraveseTarget(target);
        }

        QueryBuilder<Components...>& Cascade(EntityId first, EntityId second)
        {
            return Traverse(CASCADE).TraveseTarget(first, second);
        }

        template<typename T>
        QueryBuilder<Components...>& Modify()
        {
            Term<T>();
            m_currTerm.behavior = STRUCTURE_CHANGE;

            return *this;
        }

        Query Build() = 0;
        // {
        //     m_desc.terms[m_currTermIdx] = m_currTerm;
        //
        //     //construct query
        //     Query query;
        //     query.id = 0;
        //     query.termCount = m_currTermIdx + 1;
        //     QueryTerm* terms = m_world->m_wAllocator.Alloc<QueryTerm>(query.termCount);
        //
        //     for(uint32_t idx = 0; idx < query.termCount; ++idx)
        //     {
        //         terms[idx] = m_desc->terms[idx];
        //     }
        //
        //     if(m_desc.cache)
        //     {
        //         if(m_world->IsEntityExist(m_desc.id))
        //         {
        //             m_desc.id = m_world->GetId();
        //         }
        //
        //         EntityDesc eDesc;
        //         eDesc.id = m_desc.id;
        //         std::snprintf(eDesc.name, 16, "Query %u", m_desc.id);
        //         ComponentSet cs;
        //         cs.Init(*m_world, 1);
        //         cs[0] = EcsQueryId; 
        //         eDesc.add = std::move(cs);
        //
        //         m_world.CreateEntity(eDesc);
        //
        //         query.id = m_desc.id;
        //
        //         //filter to cache immediately
        //     }
        //
        //     return query;
        // }

    private:
        World* m_world;
        QueryDesc m_desc;
        QueryTerm m_currTerm;
        uint32_t m_currTermIdx;
        bool m_firstTerm;
    };
}
