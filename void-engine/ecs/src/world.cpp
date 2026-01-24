#include "world.h"
#include "ds/hash_map.h"

namespace ECS
{
    World* CreateWorld()
    {
        return new World();
    }

    void* Alloc(World* world, size_t size)
    {
        return world->allocator.Alloc(size);
    }



    World::World()
        : m_generatedEntityId(1)
    {
        m_store.reserve(300);
        allocator.Init();

        //MemoryArray arr;
        //arr.Init(&allocator, sizeof(uint32_t), 2);
        //std::cout << arr.GetCapacity() << std::endl;

        HashMap<uint64_t, uint64_t> map;
        map.Init(&allocator, 4);
        map.Insert(1, 20);
        map.Insert(2, 302);
        map.Insert(12, 85);
        map.Insert(22, 185);

        //std::cout << map.ContainsKey(1) << std::endl;
        //std::cout << map.ContainsKey(2) << std::endl;
        //std::cout << map.ContainsKey(3) << std::endl;
        //std::cout << map.ContainsKey(22) << std::endl;
        std::cout << map[1] << std::endl;
        std::cout << map[2] << std::endl;
        std::cout << map[12] << std::endl;
        std::cout << "================" << std::endl;
        map.Insert(12, 200);
        std::cout << map.GetValue(12) << std::endl;

        map.Insert(3, 221);
        std::cout << map.ContainsKey(3) << std::endl;
        std::cout << map.ContainsKey(1) << std::endl;
        std::cout << map.ContainsKey(2) << std::endl;
        std::cout << map.ContainsKey(12) << std::endl;
        std::cout << map.ContainsKey(22) << std::endl;

        map.Remove(12);
        std::cout << "================" << std::endl;
        std::cout << map.ContainsKey(3) << std::endl;
        std::cout << map.ContainsKey(1) << std::endl;
        std::cout << map.ContainsKey(2) << std::endl;
        std::cout << map.ContainsKey(12) << std::endl;
        std::cout << map.ContainsKey(22) << std::endl;
    }

    World::~World()
    {
    }

    Entity World::CreateEntity()
    {
        EntityId id = GenerateEntityId();
        m_entityRecords.insert({id, EntityRecord{nullptr, 0}});
        return Entity(id, this);
    }

    EntityId World::GenerateEntityId()
    {
        EntityId id = nullEntityId;
        if(!m_freeEntityIndex.empty())
        {
            id = m_freeEntityIndex[m_freeEntityIndex.size() - 1];
            m_freeEntityIndex.pop_back();
            id = ECS_INCRE_GEN_COUNT(id);
        }
        else
        {
            id = ECS_MAKE_ENTITY_ID(m_generatedEntityId++, 0);
        }

        return id;
    }

    void World::DestroyEntity(const Entity& e)
    {
        auto it = m_entityRecords.find(e.id);
        if(it != m_entityRecords.end())
        {
            SwapBackAndRemove(it->second);
            m_entityRecords.erase(e.id);
            m_freeEntityIndex.push_back(e.id);

        }

        //remove data
    }

    bool World::isEntityAlive(const Entity& e)
    {
        if(m_entityRecords.find(e.id) != m_entityRecords.end())
        {
            return true;
        }

        return false;
    }

    void World::SwapBackAndRemove(EntityRecord record)
    {
        if(!record.archetype || record.row == 0)
        {
            return;
        }

        Archetype& archetype = *(record.archetype);
        uint32_t swappingIndex = record.row - 1;
        uint32_t backIndex = archetype.count - 1;

        EntityId swappingId = archetype.data.entities[swappingIndex];
        EntityId backId = archetype.data.entities[backIndex];

        auto it = m_entityRecords.find(backId);

        if(it != m_entityRecords.end())
        {
            EntityRecord& swappedRecord = it->second;
            swappedRecord.row = record.row;

            archetype.data.entities[swappingIndex] = backId;
            archetype.data.entities[backIndex] = swappingId;
        }
        else
        {
            return;
        }

        for(uint32_t i = 0; i < archetype.componentSet.GetCount(); i++)
        {
            const ComponentColumn& col = archetype.data.columns[i];
            uint8_t* swappingAddr = col.colData + swappingIndex * col.typeInfo.size;
            uint8_t* backAddr = col.colData + backIndex * col.typeInfo.size;

            //fix this later
            uint8_t temp[256];

            if(col.typeInfo.isTriviallyCopyable)
            {
                std::memcpy(temp, swappingAddr, col.typeInfo.size);
                std::memcpy(swappingAddr, backAddr, col.typeInfo.size);
                std::memcpy(backAddr, temp, col.typeInfo.size);
            }
            else if(col.typeInfo.isMoveContructible)
            {
                col.typeHook.move(temp, swappingAddr);
                col.typeHook.move(swappingAddr, backAddr);
                col.typeHook.move(backAddr, temp);
            }
            else
            {
                col.typeHook.copy(temp, swappingAddr);
                col.typeHook.copy(swappingAddr, backAddr);
                col.typeHook.copy(backAddr, temp);
            }

            if(!col.typeInfo.isTriviallyDestructible)
            {
                col.typeHook.dtor(backAddr);
            }
        }

        auto backIt = m_entityRecords.find(backId);

        if(backIt != m_entityRecords.end())
        {
            backIt->second.row = record.row;
        }

        archetype.data.entities.pop_back();
        archetype.count--;
    }

}
