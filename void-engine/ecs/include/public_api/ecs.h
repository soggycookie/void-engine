#pragma once

namespace ECS
{
    class World;
    class Entity;

    void* Alloc(World* world, size_t size);

    World* CreateWorld();
}
