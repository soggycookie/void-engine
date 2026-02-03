#include "world.h"

namespace ECS
{

    World* CreateWorld()
    {
        auto w = new World();
        w->Init();

        return w;
    }

    void DestroyWorld(World* world)
    {
        world->Destroy();
        delete world;
    }

    Entity World::CreateEntity()
    {
        uint64_t id = m_entityIndex.GetReservedFreeId();

        bool newId = false;
        if(id == 0)
        {
            newId = true;

            while(m_entityIndex.isValidDense(++m_nextFreeId));

            id = m_nextFreeId;
        }
        Entity e(id, this);
        
        if(!newId)
        {
            e.IncreGenCount();
        }

        EntityRecord r;
        r.archetype = nullptr;
        r.dense = 0;
        r.row = 0;
        std::snprintf(r.name, 16, "Entity");

        r.dense = m_entityIndex.PushBack(e.GetFullId(), r, newId);
        GetEntityRecord(e.GetFullId())->dense = r.dense;

        return e;
    }

    Entity World::CreateEntity(const char* name)
    {
        uint64_t id = m_entityIndex.GetReservedFreeId();
        bool newId = false;
        if(id == 0)
        {
            newId = true;

            while(m_entityIndex.isValidDense(++m_nextFreeId));

            id = m_nextFreeId;
        }
        Entity e(id, this);
        
        if(!newId)
        {
            e.IncreGenCount();
        }

        EntityRecord r;
        r.archetype = nullptr;
        r.dense = 0;
        r.row = 0;
        std::snprintf(r.name, 16, name);

        r.dense = m_entityIndex.PushBack(e.GetFullId(), r, newId);
        GetEntityRecord(e.GetFullId())->dense = r.dense;

        return e;
    }

    Entity World::CreateEntity(LoEntityId id)
    {
        if(!m_entityIndex.isValidDense(id))
        {
            Entity e(id, this);

            EntityRecord r;
            r.archetype = nullptr;
            r.dense = 0;
            r.row = 0;
            std::snprintf(r.name, 16, "Entity");

            r.dense = m_entityIndex.PushBack(e.GetFullId(), r, true);
            GetEntityRecord(e.GetFullId())->dense = r.dense;
            
            return e;
        }
        else
        {
            uint64_t freeId = m_entityIndex.GetReservedFreeId();
            bool newId = false;
            if(freeId == 0)
            {
                newId = true;

                while(m_entityIndex.isValidDense(++m_nextFreeId));

                freeId = m_nextFreeId;
            }
            
            Entity e(freeId, this);
            
            if(!newId)
            {
                e.IncreGenCount();
            }
            
            EntityRecord r;
            r.archetype = nullptr;
            r.dense = 0;
            r.row = 0;
            std::snprintf(r.name, 16, "Entity");

            r.dense = m_entityIndex.PushBack(e.GetFullId(), r, newId);
            GetEntityRecord(e.GetFullId())->dense = r.dense;
            
            return e;
        }
    }

    Entity World::CreateEntity(LoEntityId id, const char* name)
    {
        if(!m_entityIndex.isValidDense(id))
        {
            Entity e(id, this);

            EntityRecord r;
            r.archetype = nullptr;
            r.dense = 0;
            r.row = 0;
            std::snprintf(r.name, 16, name);

            r.dense = m_entityIndex.PushBack(e.GetFullId(), r, true);
            GetEntityRecord(e.GetFullId())->dense = r.dense;
            
            return e;
        }
        else
        {
            uint64_t freeId = m_entityIndex.GetReservedFreeId();
            bool newId = false;
            if(freeId == 0)
            {
                newId = true;
                
                while(m_entityIndex.isValidDense(++m_nextFreeId));

                freeId = m_nextFreeId;
            }
            Entity e(freeId, this);
            
            if(!newId)
            {
                e.IncreGenCount();
            }
            
            EntityRecord r;
            r.archetype = nullptr;
            r.dense = 0;
            r.row = 0;
            std::snprintf(r.name, 16, name);

            r.dense = m_entityIndex.PushBack(e.GetFullId(), r, newId);
            GetEntityRecord(e.GetFullId())->dense = r.dense;

            return e;
        }
    }

    EntityRecord* World::GetEntityRecord(EntityId id)
    {
        EntityRecord* e = m_entityIndex.GetPageData(id);
        
        if(!e)
        {
            return nullptr;
        }
        else
        {
            return e;
        }
    }

    void World::Init()
    {
        m_wAllocator.Init();
        InitAllocators();
        m_entityIndex.Init(&m_wAllocator, nullptr, 8, true);
        m_archetypes.Init(&m_wAllocator, &m_allocators.archetypes, 8, false);
        m_componentRecords.Init(&m_wAllocator, 8);
        m_typeInfos.Init(&m_wAllocator, 8);
        m_mappedArchetype.Init(&m_wAllocator, 8);
    }

    void World::InitAllocators()
    {
        m_allocators.archetypes.Init(SparsePageCount * sizeof(Archetype));
    }

    Entity World::GetEntity(EntityId id)
    {
        EntityRecord* r = m_entityIndex.GetPageData(id);
        uint32_t dense = r->dense;
        uint64_t versionedId = m_entityIndex.GetDenseArr()[dense];
        if(!r || versionedId != id)
        {
            return Entity(0, this);    
        }

        return Entity(id, this);
    }




}