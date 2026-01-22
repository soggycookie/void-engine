#include "entity.h"
#include "world.h"

namespace VoidEngine
{
    namespace ECS
    {
        void Entity::Destroy()
        {
            world->DestroyEntity(*this);
        }
        
        bool Entity::IsAlive()
        {
            return world->isEntityAlive(*this);
        }
    }
}