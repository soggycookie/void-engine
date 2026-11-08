#pragma once
#include "../ecs/ecs_type.h"
#include "../ecs/world.h"

namespace VoidEngine::ECS
{

World *create_world(Vallocator &allocator);
void destroy_world(Vallocator &allocator, World *world);

} // namespace VoidEngine::ECS
