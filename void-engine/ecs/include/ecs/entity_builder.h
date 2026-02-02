#pragma once
#include "id.h"

namespace ECS
{
    class World;

    class EntityBuilder : public Id
    {
    protected:
        EntityBuilder(EntityId id, World* world)
            : m_world(world), Id(id)
        {
        }
        
        EntityBuilder(LoEntityId lowId, HiEntityId highId, World* world)
            : m_world(world), Id(lowId, highId)
        {
        }

        virtual ~EntityBuilder() = default;

        EntityBuilder(EntityBuilder&& other) = default; 
        EntityBuilder(const EntityBuilder& other) = default; 

        EntityBuilder& operator=(EntityBuilder&& other) = default;
        EntityBuilder& operator=(const EntityBuilder& other) = default;
    
    public:
        template<typename Component>
        EntityBuilder& AddComponent();
        
        template<typename Component>
        EntityBuilder& RemoveComponent();
        
        template<typename FirstComponent, typename... Components>
        EntityBuilder& AddComponents(const FirstComponent& f, const Components&... c);
        
        template<typename FirstComponent, typename... Components>
        EntityBuilder& RemoveComponents();

        template<typename Component>
        EntityBuilder& Set(Component&& c);

        template<typename Component>
        Component& Get();
    protected:
        World* m_world;
    };

}
