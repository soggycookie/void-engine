#pragma once
#include "../ecs/world.h"
#include "../ecs/ecs_type.h"

namespace ECS
{
    //class World;
    //class Entity;

    void* Alloc(World* world, size_t size);

    World* CreateWorld();

    void DestroyWorld(World* world);

}
