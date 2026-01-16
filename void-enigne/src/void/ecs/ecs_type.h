#pragma once
#include "void/pch.h"

namespace VoidEngine
{
    namespace ECS
    {
        using ComponentId = uint32_t;

        class ComponentSet
        {
        public:
            ComponentSet()
                : m_components(), m_hash(0), m_isHash(false)
            {
            }
            
            ComponentSet(const std::vector<ComponentId>& components)
                : m_components(components), m_hash(0), m_isHash(false)
            {
            }
            
            ComponentSet(std::vector<ComponentId>&& components)
                : m_components(std::move(components)), m_hash(0), m_isHash(false)
            {
            }

            ~ComponentSet() = default;

            ComponentSet(const ComponentSet& other)
                : m_components(other.m_components), m_hash(other.m_hash), m_isHash(other.m_isHash)
            {

            }
            
            ComponentSet(ComponentSet&& other) noexcept
                : m_components(std::move(other.m_components)), m_hash(other.m_hash), m_isHash(other.m_isHash)
            {
                other.m_hash = 0;
                other.m_isHash = false;
            }

            ComponentSet& operator=(const ComponentSet& other)
            {
                m_components = other.m_components;
                m_hash = other.m_hash;
                m_isHash = other.m_hash;          

                return *this;
            }
            
            ComponentSet& operator=(ComponentSet&& other) noexcept
            {
                m_components = std::move(other.m_components);
                m_hash = other.m_hash;
                m_isHash = other.m_hash;

                other.m_hash = 0;
                other.m_isHash = false;      

                return *this;
            }

            bool operator==(const ComponentSet& other) const
            {
                if(m_components.size() != other.m_components.size())
                {
                    return false;
                }

                for(uint32_t i = 0; i < m_components.size(); i++)
                {
                    if(m_components[i] != other.m_components[i])
                    {
                        return false;
                    }
                }

                return true;
            }

            bool operator!=(const ComponentSet& other) const
            {
                if(m_components.size() != other.m_components.size())
                {
                    return true;
                }

                for(uint32_t i = 0; i < m_components.size(); i++)
                {
                    if(m_components[i] != other.m_components[i])
                    {
                        return true;
                    }
                }

                return false;
            }

            std::vector<ComponentId>::iterator Search(ComponentId id)
            {
                return std::lower_bound(m_components.begin(), m_components.end(), id);
            }
            
            bool Contains(ComponentId id)
            {
                return std::binary_search(m_components.begin(), m_components.end(), id);
            }
            
            std::vector<ComponentId>::iterator Begin()
            {
                return m_components.begin();
            }
            
            std::vector<ComponentId>::iterator End()
            {
                return m_components.end();
            }
            
            std::vector<ComponentId>::const_iterator Search(ComponentId id) const
            {
                return std::lower_bound(m_components.begin(), m_components.end(), id);
            }
            
            std::vector<ComponentId>::const_iterator Begin() const
            {
                return m_components.begin();
            }
            
            std::vector<ComponentId>::const_iterator End() const
            {
                return m_components.end();
            }

            bool Add(ComponentId id)
            {
                auto it = Search(id);
                if(it != m_components.end() && *it == id)
                {
                    return false;
                }
                else
                {
                    m_components.push_back(id);
                    m_isHash = false;
                }

                return true;
            }

            bool Remove(ComponentId id)
            {
                auto it = std::lower_bound(m_components.begin(), m_components.end(), id);
                if(it != m_components.end() && *it == id)
                {
                    m_components.erase(it);
                    m_isHash = false;
                }
                else
                {
                    return false;
                }

                return true;
            }

            void Sort()
            {
                std::sort(m_components.begin(), m_components.end());
            }

            size_t Hash() const
            {
                if(m_isHash)
                {
                    return m_hash;
                }

                size_t h = 0;
                for(ComponentId c : m_components)
                {
                    h ^= std::hash<ComponentId>{}(c)
                        +0x9e3779b97f4a7c15ULL
                        + (h << 6)
                        + (h >> 2);
                }

                m_hash = h;
                m_isHash = true;
                return h;
            }

            ComponentId& operator[](uint32_t index)
            {
                if(index >= m_components.size())
                {
                    assert(0 && "index is out of bound!");
                }

                return m_components[index];
            }
            
            const ComponentId& operator[](uint32_t index) const
            {
                if(index >= m_components.size())
                {
                    assert(0 && "index is out of bound!");
                }

                return m_components[index];
            }

            uint32_t GetCount() const
            {
                return m_components.size();
            }

        public:
            std::vector<ComponentId> m_components;

        private:
            mutable size_t m_hash;
            mutable bool m_isHash;
        };

        using HashComponentSet = size_t;

        using EntityId = uint64_t;

        constexpr EntityId nullEntityId = 0;

        struct ComponentTypeInfo
        {
            uint32_t size;
            uint32_t alignment;

            bool isTriviallyCopyable;
            bool isTriviallyDestructible;
            bool isMoveContructible;
        };

        struct ComponentTypeHook
        {
            std::function<void(void* src)> ctor = nullptr;
            std::function<void(void* src)> dtor = nullptr;
            std::function<void(void* dest, const void* src)> copy = nullptr;
            std::function<void(void* dest, void* src)> move = nullptr;        
        };

        struct ComponentColumn
        {
            uint8_t* colData;
            //move this another map ig
            ComponentTypeInfo typeInfo;
            ComponentTypeHook typeHook;
        };

        struct ArcheTypeData
        {
            std::vector<EntityId> entities;
            std::vector<ComponentColumn> columns;
        };

        struct Archetype;
        struct ArchetypeEdge
        {
            Archetype* added;
            Archetype* removed;
        };

        struct Archetype
        {
            HashComponentSet hashId;
            uint32_t count;
            uint32_t capacity;
            ArcheTypeData data;
            ComponentSet componentSet;
            std::unordered_map<ComponentId, ArchetypeEdge> edges;
            uint32_t storeIndex;

        };

        struct ComponentIdGenerator
        {
            static uint32_t Generate()
            {
                static uint32_t id = 1;
                return id++;
            }
        };

        template<typename T>
        struct GenComponentId
        {
            static uint32_t id;
        };

        template<typename T>
        uint32_t GetComponentId()
        {
            static uint32_t id = ComponentIdGenerator::Generate();
            return id;
        }


#define ECS_ENTITY_ID_BITS      32
#define ECS_ENTITY_GEN_BITS     16
#define ECS_ENTITY_GEN_SHIFT    ECS_ENTITY_ID_BITS

#define ECS_ENTITY_ID_MASK      0xFFFFFFFFULL
#define ECS_ENTITY_GEN_MASK     0xFFFFULL

#define ECS_ENTITY_ID(x) \
    ((uint32_t)((x) & ECS_ENTITY_ID_MASK))

#define ECS_ENTITY_GEN_COUNT(x) \
    ((uint16_t)(((x) >> ECS_ENTITY_GEN_SHIFT) & ECS_ENTITY_GEN_MASK))

#define ECS_MAKE_ENTITY_ID(id, gen) \
    ((((uint64_t)(gen) & ECS_ENTITY_GEN_MASK) << ECS_ENTITY_GEN_SHIFT) | \
     ((uint64_t)(id)  & ECS_ENTITY_ID_MASK))

#define ECS_INCRE_GEN_COUNT(x) \
    ECS_MAKE_ENTITY_ID( \
        ECS_ENTITY_ID(x), \
        (uint16_t)(ECS_ENTITY_GEN_COUNT(x) + 1) \
    )

        struct EntityRecord
        {
            Archetype* archetype;
            uint32_t row;
        };

        struct ArchetypeRecord
        {
            Archetype* type;
            uint32_t column;
        };

        struct ComponentInfo
        {
            ComponentTypeInfo typeInfo;
            ComponentTypeHook typeHook;
            std::vector<ArchetypeRecord> records;


            ComponentInfo& SetCtorHook(std::function<void(void*)> ctor)
            {
                typeHook.ctor = ctor;

                return *this;
            }
            
            ComponentInfo& SetDtorHook(std::function<void(void*)> dtor)
            {
                typeHook.dtor = dtor;
                typeInfo.isTriviallyDestructible = false;
                return *this;
            }

            ComponentInfo& SetCopyHook(std::function<void(void*, const void*)> copy)
            {
                typeHook.copy = copy;
                typeInfo.isTriviallyCopyable = false;
                return *this;
            }
            
            ComponentInfo& SetMoveHook(std::function<void(void*, void*)> move)
            {
                typeHook.move = move;
                typeInfo.isMoveContructible = true;
                return *this;
            }
        };
    }
}

namespace std
{
    template<>
    struct hash<VoidEngine::ECS::ComponentSet>
    {
        size_t operator()(const VoidEngine::ECS::ComponentSet& s) const noexcept
        {
            return s.Hash();
        }
    };
}