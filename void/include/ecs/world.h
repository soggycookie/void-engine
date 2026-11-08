#pragma once
#include "dynamic_array.h"
#include "ecs_type.h"
#include "entity.h"
#include "pipeline.h"
#include "query.h"
#include "sparse_set.h"
#include "type_info.h"

namespace VoidEngine::ECS
{

struct EcsBatch
{
    Query_T *systems[128];
    World *world;
    uint32_t count;
    std::atomic<uint32_t> cursor;
    std::atomic<uint32_t> remaining;
};

class World
{
private:
    struct EntityCommandNode
    {
        EntityCommand command;
        EntityCommandNode *next;
    };

    class WorkerScratch
    {
    private:
        Arena scratch_arena;
        EntityCommandNode *head;

    public:
        WorkerScratch() : scratch_arena(), head(nullptr)
        {
        }

        WorkerScratch(WorkerScratch &&other) : scratch_arena(std::move(other.scratch_arena)), head(other.head)
        {
            other.head = nullptr;
        }

        void init();
        void destroy();
        void *push(uint64_t size, uint64_t align);
        void reset();
        void push_entity_cmd(EntityCommand &&cmd);
        EntityCommandNode *get_cmds()
        {
            return head;
        }
    };

    static thread_local int32_t t_scratch_idx;

    Vallocator vallocator;
    SparseSet<EntityRecord> entity_records;
    SparseSet<Archetype> archetypes;
    SparseSet<Query_T> cached_queries;
    SparseSet<TypeMetadata> type_metas;

    FlatHashMap<EntityId, TypeInfo> type_infos;
    FlatHashMap<TypeSet, Archetype *> archetype_map; // value hold a ref to key, does not
                                                     // change the value's key ref
    DynamicArray<EntityId> type_ids;
    DynamicArray<WorkerScratch> worker_scratches; // scratch at 0 is reserved for main thread
    EcsBatch system_batch;
    Pipeline loop_pipeline;
    uint32_t sweep_val; // use to re-evaluate cached queries
    LowEntityId next_free_eid;
    uint32_t worker_count;
    std::atomic<uint32_t> worker_round_robin;
    bool deferred;
    bool multithread;
    bool first_frame;

public:
    World()
        : next_free_eid(200), deferred(false), first_frame(false), multithread(false), sweep_val(0),
          vallocator(), type_infos(), archetype_map(), type_ids(), loop_pipeline(), worker_scratches(),
          worker_round_robin(0), worker_count(0)
    {
        system_batch.world = this;
    }

    static void run_system(void *batch);
    static void run_system_main(void *batch);

    void startup();
    void shutdown();

    void register_internal_components();

    void set_worker_count(uint32_t worker_count);
    bool is_multithread() const
    {
        return multithread;
    }
    std::atomic<uint32_t> &get_worker_round_robin()
    {
        return worker_round_robin;
    }
    uint32_t get_worker_count() const
    {
        return worker_count;
    }
    WorkerScratch &get_current_scratch();
    void reset_scratch();
    EcsBatch *get_system_batch()
    {
        return &system_batch;
    }

    void setup_default_pipeline();
    void init_pipeline_phase();

    // use ENTITY_INVALID_ID will let world choose entity id (include old id)
    Entity create_base_entity(LowEntityId eid = ENTITY_INVALID_ID);
    EntityBuilder create_entity(const char *name = nullptr, EntityId parent_id = ENTITY_INVALID_ID);
    EntityBuilder create_entity(EntityId eid, const char *name = nullptr,
                                EntityId parent_id = ENTITY_INVALID_ID);
    EntityBuilder entity_builder();

    void remove_entity(EntityId eid);
    void resolve_entity_cmd(EntityCommand &cmd);

    EntityId get_entity_full_id(LowEntityId low_eid);
    LowEntityId get_free_id();
    void consume_free_id(LowEntityId new_id);
    bool is_entity_existing(EntityId eid) const;

    EntityRecord &get_entity_record(EntityId eid);
    Entity get_entity_from_id(EntityId eId);

    void set_child_of(EntityId child_id, EntityId parent_id);
    EntityId get_parent(EntityId child_id);

    // Remove to call Destroy after
    // DynamicArray<EntityId> GetChildren(EntityId eId);

    template <typename T>
    TypeMetaBuilder<T> component();

    template <typename T>
    TypeMetaBuilder<T> tag();

    template <typename T>
    TypeMetaBuilder<T> relation();

    template <typename T>
    void register_type_metadata(TypeMetadata &&type_meta, const std::string_view name);

    void register_relationship(EntityId relation_id, EntityId target_eid,
                               const TypeMetadata *relation_tm);
    bool is_type_registerd(EntityId tid) const;

    template <typename T>
    void add_component(EntityId eid);

    template <typename T>
    void assign_component(EntityId eid, T &&data);

    template <typename T>
    void assign_relationship(EntityId eid, EntityId target_eid, T &&data);

    template <typename T>
    void remove_type(EntityId eid);

    template <typename T>
    void add_relation(EntityId eid, EntityId target_eid);

    template <typename T>
    void add_tag(EntityId eid);

    template <typename T>
    bool has_type(EntityId eid);

    void add_component(EntityId eid, EntityId tid);
    void assign_component_move(EntityId eid, EntityId tid, void *data);
    void assign_component_copy(EntityId eid, EntityId tid, const void *data);

    void add_relationship(EntityId eid, EntityId relation_id, EntityId target_eid);
    void assign_relationship_move(EntityId eid, EntityId relation_id, EntityId target_eid, void *data);
    void assign_relationship_copy(EntityId eid, EntityId relation_id, EntityId target_eid,
                                  const void *data);

    void add_tag(EntityId eid, EntityId tid);

    void remove_type(EntityId eid, EntityId tid);

    bool has_type(EntityId eid, EntityId tid);

    template <typename T>
    void set(EntityId eid, T &&c);

    template <typename T>
    T &get_mut(EntityId eid);

    template <typename T>
    const T &get(EntityId eid) const;

    template <typename T>
    T &get_singleton();

    void set_move(EntityId eid, EntityId tid, void *data);
    void set_copy(EntityId eid, EntityId tid, const void *data);

    void *get(EntityId eid, EntityId tid);

    void swap_back_entity(EntityRecord &r);
    void update_type_info_tracked_queries(Query_T *query);
    Query_T *create_query(uint32_t query_id);

    Archetype *create_archetype(TypeSet &&type_set);
    Archetype *get_archetype(const TypeSet &type_set);
    const Archetype *get_entity_archetype(EntityId eid) const;

    Archetype *get_or_create_archetype_add(Archetype *src_archetype, EntityId tid);
    Archetype *get_or_create_archetype_remove(Archetype *src_archetype, EntityId tid);

    void move_archetype_add_op(EntityId eid, EntityRecord &r, Archetype *dst_archetype);
    void move_archetype_move_op(EntityId eid, EntityRecord &r, Archetype *dst_archetype,
                                void *data = nullptr);
    void move_archetype_copy_op(EntityId eid, EntityRecord &r, Archetype *dst_archetype,
                                const void *data = nullptr);
    void move_archetype_remove_op(EntityId eid, EntityRecord &r, Archetype *dst_archetype);

    void move_to(void *dst, void *src, const TypeMetadata *tm);
    void copy_to(void *dst, const void *src, const TypeMetadata *tm);

    // Query

    template <typename... T>
    QueryBuilder<T...> query_builder();

    template <typename... T>
    SystemBuilder<T...> system_builder();

    enum class EntityRevalidationMode
    {
        ON_ADDED,
        ON_REMOVED,
        ON_MODIFIED,
    };

    void revalidate_cached_query_entity_filter(Archetype *archetype, uint32_t affected_row,
                                               EntityRevalidationMode mode);

    // if newArchetype = false -> archetype is removed
    void revalidate_cached_query_archetype_filter(TypeInfo &type_info, Archetype *archetype,
                                                  bool new_archetype);

    PhaseDependencyBuilder create_bootstrap_phase_dependency();
    PhaseDependencyBuilder create_loop_phase_dependency();

    void start_defer()
    {
        deferred = true;
    }
    void end_defer()
    {
        deferred = false;
        flush_deferred_cmds();
    }

    void tick();
    void flush_deferred_cmds();

    const FlatHashMap<EntityId, TypeInfo> &get_type_infos() const
    {
        return type_infos;
    }

    Vallocator &get_vallocator()
    {
        return vallocator;
    }
};
} // namespace VoidEngine::ECS

#include "sparse_set.inl"
#include "entity.inl"
#include "query.inl"
#include "type_info.inl"
#include "world.inl"
