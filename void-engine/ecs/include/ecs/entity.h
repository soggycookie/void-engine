#pragma once
#include "ecs_type.h"


namespace ECS
{
    class World;

    class Entity
    {
    private:
        friend class World;

        Entity(EntityId id, World* scene)
            :id(id), world(scene), name()
        {
            std::ostringstream oss;
            oss << "Entity " << ECS_ENTITY_ID(id);
            name = std::move(oss.str());
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

        EntityId GetFullId() const
        {
            return id;
        }

        template<typename T>
        void Add(const T& data);

        template<typename T>
        void Remove();

        template<typename T>
        T* Get();

    private:
        EntityId id;
        World* world;
        std::string name;
    };
}


namespace std
{
    template<>
    struct hash<ECS::Entity>
    {
        size_t operator()(const ECS::Entity& e) const noexcept
        {
            return std::hash<ECS::EntityId>{}(e.GetFullId());
        }
    };
}