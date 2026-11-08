#pragma once
#include "flat_hash_map.h"
#include "ecs_type.h"
#include "internal_component.h"
#include "query.h"
#include "type_info.h"

namespace VoidEngine::ECS
{

////////////////////////// Pipeline ///////////////////////////
class Pipeline
{
private:
    struct Step
    {
        Query_T *system;
        bool sync_point;
    };

    using Stage = DynamicArray<Step>;
private:
    DynamicArray<Stage> stages_;
public:
    Pipeline() : stages_() {}

    void destroy(Vallocator &vallocator);
    void init(Vallocator &vallocator);
    void build_from_base_phase(World * world, EntityId phase_id);
    void build_step(World *world, FlatHashMap<EntityId, TermAccess> &accesses, System *system,
                   Stage &appending_stage);

    void progress(World *world);
};

////////////////////// PhaseDependencyBuilder ///////////////////////

class PhaseDependencyBuilder
{
public:
    PhaseDependencyBuilder(World *world, EntityId phase_id);

    PhaseDependencyBuilder &depend_on(EntityId eid, const char *name);

private:
    EntityId phase_id;
    EntityId active_phase_id;
    World *world;
};

} // namespace VoidEngine::ECS
