#pragma once
#include "void/pch.h"
#include "ecs_type.h"

namespace VoidEngine
{
    namespace ECS
    {
        class World;

        class Entity
        {
        private:
            friend class World;

            Entity(EntityId id, World* scene)
                :id(id), scene(scene)
            {
            }

        public:
            void Destroy();
            bool IsAlive();
            
            uint32_t GetId() const
            {
                return ECS_ENTITY_ID(id);
            }
            
            uint32_t GetGenCount() const
            {
                return ECS_ENTITY_GEN_COUNT(id);
            }

            template<typename T>
            void Add(const T& data);

            template<typename T>
            void Remove();

            template<typename T>
            T* Get();

        private:
            EntityId id;
            World* scene;
        };

        class World
        {
        public:
            World();
            ~World();

            Entity CreateEntity();
            void DestroyEntity(EntityId id);
            bool isEntityAlive(EntityId id);
            ////////////////////////////////////////////////////////////////

            //NOTE: WE HAVE TO REWRITE THEM MOSTLY TO DEFER THE ARCHETYPE CHANGE

            ////////////////////////////////////////////////////////////////

            template<typename T>
            T* Get(EntityId id)
            {
                auto it = m_entityRecords.find(id);

                if(it != m_entityRecords.end())
                {
                    Archetype* archetype = it->second.archetype;

                    auto componentIt = archetype->componentSet.Search(GetComponentId<T>());
                    if(componentIt != archetype->componentSet.End())
                    {
                        uint32_t colIndex = componentIt - archetype->componentSet.Begin();
                        T* colData = reinterpret_cast<T*>(archetype->data.columns[colIndex].colData);

                        return &colData[it->second.row - 1];
                    }
                    assert(0 && "Component does not exist!");
                }
                assert(0 && "Entity does not exist!");
                
                return nullptr;
            }

//#define ECS_AUTO_REGISTER

            template<typename T>
            void Add(EntityId id, const T& data)
            {
                if(m_componentIndex.find(GetComponentId<T>()) == m_componentIndex.end())
                {
#ifdef ECS_AUTO_REGISTER
                    Register<T>();
#else
                    assert(0 && "Component is not registered!");
#endif
                }

                auto it = m_entityRecords.find(id);
                if(it == m_entityRecords.end())
                {
                    assert(0 && "Entity does not exit!");
                    return;
                }

                EntityRecord& record = it->second;
                Archetype* srcArchetype = record.archetype;
                Archetype* destArchetype = ResolveDestArchetype<T>(srcArchetype, true);


                if(!destArchetype)
                {
                    assert(0 && "destArchetype is nullptr!");
                }

                if(destArchetype->capacity == destArchetype->count)
                {
                    EnsureCapacity(*destArchetype, 1);
                }

                SwapBack(record);
                for(uint32_t i = 0; i < destArchetype->componentSet.GetCount(); i++)
                {
                    const ComponentColumn& destCol = destArchetype->data.columns[i];
                    uint8_t* destAddr = destArchetype->data.columns[i].colData +
                        destArchetype->count * destCol.typeInfo.size;

                    if(!srcArchetype)
                    {
                        if(GetComponentId<T>() == destArchetype->componentSet[i])
                        {
                            if(destCol.typeInfo.isTriviallyCopyable)
                            {
                                std::memcpy(destAddr, &data, destCol.typeInfo.size);
                            }
                            else
                            {
                                destCol.typeHook.copy(destAddr, &data);
                            }
                        }
                    }
                    else
                    {
                        auto it = srcArchetype->componentSet.Search(destArchetype->componentSet[i]);

                        if(it == srcArchetype->componentSet.End())
                        {
                            //placeholder
                            if(GetComponentId<T>() == destArchetype->componentSet[i])
                            {
                                if(destCol.typeInfo.isTriviallyCopyable)
                                {
                                    std::memcpy(destAddr, &data, destCol.typeInfo.size);
                                }
                                else
                                {
                                    destCol.typeHook.copy(destAddr, &data);
                                }
                            }
                        }
                        else
                        {
                            uint32_t srcColIndex = it - srcArchetype->componentSet.Begin();
                            const ComponentColumn& srcCol = srcArchetype->data.columns[srcColIndex];
                            //swap back
                            uint8_t* srcAddr = srcCol.colData + (srcArchetype->count - 1) *
                                srcCol.typeInfo.size;

                            if(destCol.typeInfo.isTriviallyCopyable)
                            {
                                std::memcpy(destAddr, srcAddr, destCol.typeInfo.size);
                            }
                            else if(destCol.typeInfo.isMoveContructible)
                            {
                                destCol.typeHook.move(destAddr, srcAddr);
                            }
                            else
                            {
                                destCol.typeHook.copy(destAddr, srcAddr);
                            }

                            if(!destCol.typeInfo.isTriviallyDestructible)
                            {
                                destCol.typeHook.dtor(srcAddr);
                            }
                        }
                    }
                }

                if(srcArchetype)
                {
                    srcArchetype->data.entities.pop_back();
                    srcArchetype->count--;
                }

                destArchetype->data.entities.push_back(id);
                destArchetype->count++;

                record.archetype = destArchetype;
                record.row = destArchetype->count;
            }

            template<typename T>
            void Remove(EntityId id)
            {
                if(m_componentIndex.find(GetComponentId<T>()) == m_componentIndex.end())
                {
#ifdef ECS_AUTO_REGISTER
                    Register<T>();
#else
                    assert(0 && "Component is not registered!");
#endif
                }

                auto it = m_entityRecords.find(id);
                if(it == m_entityRecords.end())
                {
                    assert(0 && "Entity does not exit!");
                    return;
                }

                EntityRecord record = it->second;
                Archetype* srcArchetype = record.archetype;
                Archetype* destArchetype = ResolveDestArchetype<T>(srcArchetype, false);

                if(!destArchetype)
                {
                    assert(0 && "destArchetype is nullptr!");
                }

                if(destArchetype->capacity == destArchetype->count)
                {
                    EnsureCapacity(*destArchetype, srcArchetype->count);
                }

                SwapBack(record);
                for(uint32_t i = 0; i < srcArchetype->componentSet.GetCount(); i++)
                {
                    const ComponentColumn& srcCol = srcArchetype->data.columns[i];
                    //swap back
                    uint8_t* srcAddr = srcCol.colData +
                        (srcArchetype->count - 1) * srcCol.typeInfo.size;

                    auto it = destArchetype->componentSet.Search(srcArchetype->componentSet[i]);

                    if(it != destArchetype->componentSet.End())
                    {
                        uint32_t destColIndex = it - destArchetype->componentSet.Begin(); 
                        const ComponentColumn& destCol = destArchetype->data.columns[destColIndex];
                        uint8_t* destAddr = destCol.colData +
                            destArchetype->count * destCol.typeInfo.size;

                        if(destCol.typeInfo.isTriviallyCopyable)
                        {
                            std::memcpy(destAddr, srcAddr, destCol.typeInfo.size);
                        }
                        else if(destCol.typeInfo.isMoveContructible)
                        {
                            destCol.typeHook.move(destAddr, srcAddr);
                        }
                        else
                        {
                            destCol.typeHook.copy(destAddr, srcAddr);
                        }

                        if(!destCol.typeInfo.isTriviallyDestructible)
                        {
                            destCol.typeHook.dtor(srcAddr);
                        }
                    }
                }
                


                srcArchetype->data.entities.pop_back();
                srcArchetype->count--;
                destArchetype->data.entities.push_back(id);
                destArchetype->count++;

                record.archetype = destArchetype;
                record.row = destArchetype->count;
            }

            template<typename T>
            ComponentInfo& Register()
            {
                auto it = m_componentIndex.find(GetComponentId<T>());

                if(it != m_componentIndex.end())
                {
                    return it->second;
                }

                static_assert(std::is_default_constructible_v<T>,
                              "Component must be default constructible");

                static_assert(std::is_copy_constructible_v<T>,
                              "Component must be copyable");

                if constexpr (!std::is_default_constructible_v<T>)
                {
                    assert(0 && "Component contructor should take no args!");
                }
                ComponentInfo componentInfo;
                componentInfo.typeInfo =
                {
                    //should be aligned size
                    sizeof(T),
                    alignof(T),
                    std::is_trivially_copyable_v<T>,
                    std::is_trivially_destructible_v<T>,
                    std::is_move_constructible_v<T> && !std::is_trivially_move_constructible_v<T>                
                };

                componentInfo.typeHook.ctor =
                    [](void* ptr)
                    {
                        new (ptr) T();
                    };

                if constexpr(!std::is_trivially_destructible_v<T>)
                {
                    componentInfo.typeHook.dtor = 
                        [](void* ptr)
                        {
                            static_cast<T*>(ptr)->~T();
                        };
                }

                if constexpr(!std::is_trivially_copyable_v<T>)
                {
                    componentInfo.typeHook.copy =
                        [](void* dst, const void* src)
                        {
                            new (dst) T(*static_cast<const T*>(src));
                        };
                }

                if constexpr(std::is_move_constructible_v<T> &&
                             !std::is_trivially_move_constructible_v<T>)
                {
                    componentInfo.typeHook.move =
                        [](void* dst, void* src)
                        {
                            new (dst) T(std::move(*static_cast<T*>(src)));
                        };
                }

                m_componentIndex.insert({GetComponentId<T>(), componentInfo});

                return componentInfo;
            }

            /// <summary>
            /// use in editor ? NOT FINISHED
            /// </summary>
            /// <typeparam name="T"></typeparam>

            template<typename T>
            void Unregister()
            {
                //NOTE: cascade erase archetype, component data

                auto it = m_componentIndex.find(GetComponentId<T>());
                if(it != m_componentIndex.end())
                {
                    auto& archetypeRecords = it->second;

                    for(ArchetypeRecord& record : archetypeRecords)
                    {
                        Archetype& removedArchetype = *(record.type);

                        //NOTE:: handle special type of Component that hold resource
                        //leave open for implementing type hook

                        if constexpr(!std::is_trivially_destructible_v<T>)
                        {
                            if(removedArchetype.data.columns.size() - 1 >= record.column)
                            {
                                ComponentColumn& col = removedArchetype.data.columns[record.column];
                                for(uint32_t i = 0; i < removedArchetype.count; i++)
                                {
                                    T* element = static_cast<T*>(col.colData + i * col.typeInfo.size);
                                    element->~T();
                                }
                            }
                        }

                        //TODO: Free component data array

                        //merge with already exist archetype or create a new one
                        auto edgeIter = removedArchetype.edges.find(GetComponentId<T>());
                        if(edgeIter != m_componentIndex.end())
                        {
                            ArchetypeEdge edge = edgeIter->second;

                            //must validate componentSet

                            if(!removedArchetype.componentSet.m_components.size() == 1)
                            {
                                if(!edge.removed)
                                {
                                    //how to handle this lazy loading case?

                                }

                                Archetype& movedArchetype = *(edge.removed);
                                uint32_t movedArchetypeCol = 0;
                                uint32_t remainingSlot = movedArchetype.capacity - movedArchetype.count;

                                if(remainingSlot < removedArchetype.count)
                                {
                                    //reallocate
                                    AllocateArchetypeData(movedArchetype, remainingSlot);
                                }

                                for(uint32_t i = 0; i < removedArchetype.data.columns.size(); i++)
                                {
                                    if(i == record.column)
                                    {
                                        continue;
                                    }

                                    ComponentColumn& destCol = movedArchetype.data.columns[movedArchetypeCol];

                                    uint8_t* destAddr = destCol.colData + movedArchetype.count * destCol.typeInfo.size;
                                    uint8_t* srcAddr = removedArchetype.data.columns[i].colData;
                                    if(destCol.typeInfo.isTriviallyCopyable)
                                    {
                                        std::memmove(destAddr,
                                                     srcAddr,
                                                     removedArchetype.count * destCol.typeInfo.size
                                        );
                                    }
                                    else if(destCol.typeInfo.isMoveContructible)
                                    {
                                        for(uint32_t j = 0; j < removedArchetype.count; j++)
                                        {
                                            destCol.typeHook.move(destAddr, srcAddr);
                                            destCol.typeHook.dtor(srcAddr);
                                        }
                                    }
                                    else
                                    {
                                        for(uint32_t j = 0; j < removedArchetype.count; j++)
                                        {
                                            destCol.typeHook.copy(destAddr, srcAddr);
                                            destCol.typeHook.dtor(srcAddr);
                                        }
                                    }

                                    movedArchetypeCol++;
                                }

                                movedArchetype.count += removedArchetype.count;
                            }


                            FreeArchetypeData(removedArchetype);

                        }
                        else
                        {
                            //SHOULD NEVER BE HERE
                            assert(0 && "Unregister Component does not exist in archetype!");
                        }

                        m_mappedArchetype.erase(removedArchetype.componentSet);
                        EraseArchetype(record.type);
                        //m_store.erase(removedArchetype);
                    }
                    m_componentIndex.erase(GetComponentId<T>());
                }
            }

            template<typename T>
            void Each(std::function<void(Entity, T&)> func)
            {
                auto it = m_componentIndex.find(GetComponentId<T>());

                if(it == m_componentIndex.end())
                {
                    assert(0 && "Component is not registered!");
                }

                const std::vector<ArchetypeRecord>& archetypeList = it->second.records;

                for(uint32_t i = 0; i < archetypeList.size(); i++)
                {
                    Archetype* archetype = archetypeList[i].type;
                    const std::vector<EntityId>& entities = archetype->data.entities;
                    std::vector<ComponentColumn>& cols = archetype->data.columns;
                    for(uint32_t entityIndex = 0; entityIndex < entities.size(); entityIndex++)
                    {
                        Entity e(entities[entityIndex], this);
                        T* componentData = reinterpret_cast<T*>(cols[archetypeList[i].column].colData + 
                                                                entityIndex * cols[archetypeList[i].column].typeInfo.size);
                        func(e, *componentData);
                    }
                }
            }

        private:

            template<typename T>
            Archetype* ResolveDestArchetype(Archetype* srcArchetype, bool isAdded)
            {
                Archetype* destArchetype = nullptr;
                ComponentSet destComponentSet;
                if(srcArchetype)
                {
                    auto edgeIt = srcArchetype->edges.find(GetComponentId<T>());
                    //edge exist
                    if(edgeIt != srcArchetype->edges.end())
                    {
                        ArchetypeEdge& edge = edgeIt->second;

                        destArchetype = isAdded ? edge.added : edge.removed;

                        //archetype not exist
                        if(!destArchetype)
                        {
                            //lazy load
                            destComponentSet = srcArchetype->componentSet;

                            if(isAdded)
                            {
                                if(destComponentSet.Add(GetComponentId<T>()))
                                {
                                    destComponentSet.Sort();
                                }

                            }
                            else
                            {
                                if(destComponentSet.Remove(GetComponentId<T>()))
                                {
                                    destComponentSet.Sort();
                                }
                            }

                            auto mappedArchetypeIt = m_mappedArchetype.find(destComponentSet);

                            if(mappedArchetypeIt != m_mappedArchetype.end())
                            {
                                destArchetype = mappedArchetypeIt->second;
                            }
                            else
                            {
                                destArchetype = CreateAndGetArchetype<T>(destComponentSet);
                            }

                            if(isAdded)
                            {
                                edge.added = destArchetype;
                            }
                            else
                            {
                                edge.removed = destArchetype;
                            }
                        }
                        //archetype exist
                        else
                        {
                            //NOTE: there maybe invalid ptr
                            destComponentSet = destArchetype->componentSet;
                        }
                    }
                    //edge does not exist yet
                    else
                    {
                        //lazy load

                        destComponentSet = srcArchetype->componentSet;
                        if(isAdded)
                        {
                            if(destComponentSet.Add(GetComponentId<T>()))
                            {
                                destComponentSet.Sort();
                            }
                            else
                            {
                                assert(0 && "Added component already exist!");
                            }
                        }
                        else
                        {
                            if(destComponentSet.Remove(GetComponentId<T>()))
                            {
                                destComponentSet.Sort();
                            }
                            else
                            {
                                assert(0 && "Removed component does not exist!");
                            }
                        }

                        auto mappedArchetypeIt = m_mappedArchetype.find(destComponentSet);

                        if(mappedArchetypeIt != m_mappedArchetype.end())
                        {
                            destArchetype = mappedArchetypeIt->second;
                        }
                        else
                        {
                            //Create Archetype
                            destArchetype = CreateAndGetArchetype<T>(destComponentSet);
                        }

                        ArchetypeEdge edge;
                        edge.added = isAdded ? destArchetype : nullptr;
                        edge.removed = isAdded ? nullptr : destArchetype;

                        srcArchetype->edges.insert({GetComponentId<T>(), edge});
                    }
                }
                //entity not contains any archetype
                else
                {
                    if(!isAdded)
                    {
                        assert(0 && "Removed component does not exist!");
                    }

                    destComponentSet = std::move(ComponentSet({GetComponentId<T>()}));
                    auto mappedArchetypeIt = m_mappedArchetype.find(destComponentSet);

                    if(mappedArchetypeIt != m_mappedArchetype.end())
                    {
                        destArchetype = mappedArchetypeIt->second;
                    }
                    else
                    {
                        //Create Archetype
                        destArchetype = CreateAndGetArchetype<T>(destComponentSet);
                    }
                }

                return destArchetype;
            }

            void SwapBack(EntityRecord record)
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
                }
                
                auto backIt = m_entityRecords.find(backId);

                if(backIt != m_entityRecords.end())
                {
                    backIt->second.row = record.row;
                }

                auto swapit = m_entityRecords.find(swappingId);

                if(swapit != m_entityRecords.end())
                {
                    swapit->second.row = archetype.count;
                }
            }

#define ECS_DEFAULT_COMPONENT_CAPACITY 4

            template<typename T>
            Archetype* CreateAndGetArchetype(const ComponentSet& components)
            {
                //auto mappedArchetypeIt = m_mappedArchetype.find(components);

                //if(mappedArchetypeIt != m_mappedArchetype.end())
                //{
                //    return mappedArchetypeIt->second;
                //}

                Archetype* newArchetype = new Archetype();
                newArchetype->hashId = components.Hash();
                newArchetype->count = 0;
                newArchetype->capacity = 0;
                newArchetype->storeIndex = 0;
                newArchetype->componentSet = components;
                newArchetype->edges = {};
                newArchetype->data.entities = {};
                newArchetype->data.columns = {};
                newArchetype->data.columns.resize(components.GetCount());

                m_store.push_back(newArchetype);
                newArchetype->storeIndex = m_store.size() - 1;
                m_mappedArchetype[components] = newArchetype;

                for(uint32_t i = 0; i < components.GetCount(); i++)
                {
                    m_componentIndex[components[i]].records.push_back(
                        {
                            newArchetype,
                            i
                        }
                    );

                    newArchetype->data.columns[i].typeInfo = 
                        m_componentIndex[components[i]].typeInfo;

                    newArchetype->data.columns[i].typeHook = 
                        m_componentIndex[components[i]].typeHook;
                }

                AllocateArchetypeData(*newArchetype, ECS_DEFAULT_COMPONENT_CAPACITY);

                return newArchetype;
            }

#define ECS_ADDED_GROW_RATIO 0.5f
            void EnsureCapacity(Archetype& archetype, uint32_t addedCount)
            {
                uint32_t totalCount = archetype.count + addedCount;

                if(archetype.capacity >= totalCount)
                {
                    return;
                }

                //can be overflow 
                uint32_t capacity = archetype.capacity == 0 ? 1 : archetype.capacity;

                float ratio = static_cast<float>(totalCount) / static_cast<float>(capacity);
                ratio += ECS_ADDED_GROW_RATIO;

                uint32_t newCapacity = std::round(capacity * ratio);

                AllocateArchetypeData(archetype, newCapacity);
            }

            //TODO: change to allocate per column
            void AllocateArchetypeData(Archetype& archetype, uint32_t capacity)
            {
                if(archetype.capacity >= capacity)
                {
                    return;
                }

                std::vector<uint8_t*> reallocAddrs(archetype.componentSet.GetCount(), nullptr);
                uint32_t dataExistCount = 0;
                for(uint32_t i = 0; i < archetype.componentSet.GetCount(); i++)
                {
                    uint8_t* data = nullptr;
                    ComponentColumn& col = archetype.data.columns[i];
                    size_t totalSize = capacity * col.typeInfo.size;
#if defined(_MSC_VER)
                    data = static_cast<uint8_t*>(_aligned_malloc(totalSize, col.typeInfo.alignment));
#else
                    data = static_cast<uint8_t*>(std::aligned_alloc(col.typeInfo.alignment, totalSize));
#endif       
                    if(!data)
                    {
                        assert(0 && "Fail to allocate column data!");
                    }

                    if(col.colData)
                    {
                        ++dataExistCount;
                        reallocAddrs[i] = data;
                    }
                    else
                    {
                        col.colData = data;
                    }
                }

                if(dataExistCount == archetype.componentSet.GetCount())
                {
                    for(uint32_t i = 0; i < archetype.componentSet.GetCount(); i++)
                    {
                        ComponentColumn& col = archetype.data.columns[i];
                        uint8_t* srcAddr = col.colData;
                        uint8_t* destAddr = reallocAddrs[i];

                        if(col.typeInfo.isTriviallyCopyable)
                        {
                            std::memcpy(destAddr, srcAddr, archetype.count * col.typeInfo.size);
                            continue;
                        }
                        else if(col.typeInfo.isMoveContructible)
                        {
                            if(col.typeHook.move)
                            {
                                for(uint32_t element = 0; element < archetype.count; element++)
                                {
                                    uint8_t* elementSrcAddr = srcAddr + element * col.typeInfo.size;
                                    uint8_t* elementDestAddr = destAddr + element * col.typeInfo.size;
                                    col.typeHook.move(elementDestAddr, elementSrcAddr);

                                    if(!col.typeInfo.isTriviallyDestructible && col.typeHook.dtor)
                                    {
                                        col.typeHook.dtor(elementSrcAddr);
                                    }
                                }
                            }
                            else
                            {
                                assert(0 && "Move hook missing!");
                            }
                        }
                        else
                        {
                            if(col.typeHook.copy)
                            {
                                for(uint32_t element = 0; element < archetype.count; element++)
                                {
                                    uint8_t* elementSrcAddr = srcAddr + element * col.typeInfo.size;
                                    uint8_t* elementDestAddr = destAddr + element * col.typeInfo.size;
                                    col.typeHook.copy(elementDestAddr, elementSrcAddr);

                                    if(!col.typeInfo.isTriviallyDestructible && col.typeHook.dtor)
                                    {
                                        col.typeHook.dtor(elementSrcAddr);
                                    }
                                }
                            }
                            else
                            {
                                assert(0 && "Copy hook missing!");
                            }
                        }

#if defined(_MSC_VER)
                        _aligned_free(col.colData);
#else
                        std::free(col.colData);
#endif

                        col.colData = reallocAddrs[i];
                    }
                }

                archetype.capacity = capacity;
            }

            void FreeArchetypeData(Archetype& archetype)
            {
                for(uint32_t i = 0; i < archetype.componentSet.GetCount(); i++)
                {
                    ComponentColumn& col = archetype.data.columns[i];
#if defined(_MSC_VER)
                    _aligned_free(col.colData);
#else
                    std::free(col.colData);
#endif              
                }
            }

            void EraseArchetype(Archetype* type) {}

            EntityId GenerateEntityId();

        private:
            std::vector<EntityId> m_freeEntityIndex;
            std::unordered_map<EntityId, EntityRecord> m_entityRecords;
            std::unordered_map<ComponentId, ComponentInfo> m_componentIndex;
            std::unordered_map<ComponentSet, Archetype*> m_mappedArchetype;
            std::vector<Archetype*> m_store;
            uint32_t m_generatedEntityId;
        };

        template<typename T>
        void Entity::Add(const T& data)
        {
            scene->Add<T>(id, data);
        }

        template<typename T>
        void  Entity::Remove()
        {
            scene->Remove<T>(id);
        }

        template<typename T>
        T* Entity::Get()
        {
            return scene->Get<T>(id);
        }

    }
}



