#pragma once
#include "ecs_type.h"
#include "entity.h"
#include "type_info.h"


//TODO: Normal query is unsafe, because it may query stale data (sync point not flush yet)
namespace VoidEngine::ECS
{

enum TraverseMethod : uint16_t
{
    SELF,
    UP,
    CASCADE
};

enum TermOp : uint16_t
{
    HAS,
    NOT,
};

enum TermAccess : uint16_t
{
    READ,
    WRITE,
    STRUCTURE_CHANGE
};

class Query_T;

// =========================================================
//
//                      ** QueryTerm **
//
//  Term to describe a query.
//
//  Term operation:
//      - With:
//          + SELF: has component on SELF.
//          + UP: has component on Parent.
//          + CASCADE: has component on Ancestor.
//      - Without: opposite with With.
//
//  Term behavior:
//      - Query is used for System:
//          + Insert sync point.
//          + Help setting up scheduler.
//      - Query is used for normal usage, mostly run
//      discretely without any dependency:
//          + Mean nothing, because user must manually flush
//          anyway.
//
// =========================================================

struct QueryTerm
{
    QueryTerm()
        : tid(ENTITY_INVALID_ID), trav_relation(ENTITY_INVALID_ID), trav_target(ENTITY_INVALID_ID),
          valid_trav_target(ENTITY_INVALID_ID), trav_method(SELF), op(HAS), access(TermAccess::READ),
          padding(0)
    {
    }

    EntityId tid;
    EntityId trav_relation;
    EntityId trav_target;
    EntityId valid_trav_target;
    TraverseMethod trav_method;
    TermOp op;
    TermAccess access;
    uint16_t padding;
};

constexpr uint16_t INLINE_ARR_COUNT = 8;

// =========================================================
//
//                    ** MatchedArchetype **
//
//  A archetype that is matched by a query.
//
//  Its entity can be filterd if query enable entity filter.
//
// =========================================================

struct MatchedArchetype
{
    MatchedArchetype()
        : hi_term_idx_to_col_idx(nullptr), hi_entity_mask(nullptr), hi_mask_capacity(0), lo_entity_mask{0},
          lo_term_idx_to_col_idx{0}, mapped_count(0)
    {
    }

    MatchedArchetype(const Archetype *archetype)
        : archetype(archetype), hi_entity_mask(nullptr), hi_mask_capacity(0), hi_term_idx_to_col_idx(nullptr),
          lo_entity_mask{0}, lo_term_idx_to_col_idx{0}, mapped_count(0)
    {
    }

    MatchedArchetype(MatchedArchetype &&other) noexcept
        : archetype(other.archetype), hi_term_idx_to_col_idx(other.hi_term_idx_to_col_idx),
          hi_entity_mask(other.hi_entity_mask), hi_mask_capacity(other.hi_mask_capacity),
          mapped_count(other.mapped_count)
    {
        std::memcpy(lo_term_idx_to_col_idx, other.lo_term_idx_to_col_idx,
                    sizeof(int32_t) * INLINE_ARR_COUNT);
        std::memcpy(lo_entity_mask, other.lo_entity_mask,
                    sizeof(int32_t) * INLINE_ARR_COUNT);
        other.hi_entity_mask = nullptr;
        other.hi_term_idx_to_col_idx = nullptr;
        other.hi_mask_capacity = 0;
        other.mapped_count = 0;
    }

    // void Move(Vallocator &vallocator, MatchedArchetype &&other);

    void set_term_idx_to_col_idx(Vallocator &vallocator, int32_t *mapped_cols, uint32_t count);
    int32_t get_column_idx(uint32_t term_idx) const;

    void destroy(Vallocator &vallocator);

    void allocate_mask(Vallocator &vallocator);
    void set_mask(Vallocator &vallocator, uint32_t eidx, bool bit);
    bool get_mask(uint32_t eidx);

    uint32_t &mask(uint32_t eidx);

    static constexpr const uint32_t EntityPerMask = 32;

    const Archetype *archetype;
    int32_t *hi_term_idx_to_col_idx; // map term index to column index
    int32_t lo_term_idx_to_col_idx[INLINE_ARR_COUNT];
    uint32_t lo_entity_mask[INLINE_ARR_COUNT];
    uint32_t *hi_entity_mask;
    uint32_t hi_mask_capacity;
    uint32_t mapped_count;
};

// =========================================================
//
//                    ** QueryResult **
//
//  Result return by a query.
//
// =========================================================

struct QueryResult
{
    QueryResult() : archetypes() {}

    QueryResult(QueryResult &&other) noexcept
        : archetypes(std::move(other.archetypes))
    {
    }

    MatchedArchetype &add(Vallocator &vallocator, const Archetype *archetype);
    void clear(Vallocator &vallocator);
    void init(Vallocator& vallocator);
    void destroy(Vallocator &vallocator);

    MatchedArchetype &operator[](uint64_t idx)
    {
        return archetypes[idx];
    }

    DynamicArray<MatchedArchetype> archetypes;
};

// =========================================================
//
//                    ** QueryIter **
//
//  Pass as an argument to filter and exec callback of a
//  query
//
//  Contains general world context
//
// =========================================================

struct QueryIter
{
    QueryIter(World *world, EntityId eId = 0, void *ctx = nullptr, double delta_time = 0)
        : world(world), ctx(ctx), eid(eId), delta_time(delta_time)
    {
    }

    World *world;
    void *ctx;
    EntityId eid;
    double delta_time;

    Entity get_entity() { return Entity(world, eid); }
};

template <typename... CallbackArgs>
constexpr bool has_one_query_iter_v =
    ((std::is_same_v<std::decay_t<CallbackArgs>, QueryResult> ? 1 : 0) + ...) == 1;

template <typename First, typename... Rest>
constexpr bool is_first_arg_query_iter_v = std::is_same_v<First, const QueryIter &>;

// Map callmatched.matchedColummatched.matchedColumns signature to query term
// index
constexpr int32_t QUERY_ITER_ID = -1;

// =========================================================
//
//                    ** QueryExecCallback **
//
//  Exec callback of a query.
//
//  This is an abstract struct for exec callback that may
//  have multiple parameter signatures.
//
//  Also support shuffled callback sig without following
//  query term order.
//
// =========================================================

struct QueryExecCallback
{
    QueryExecCallback()
        : ctx(nullptr), fn(nullptr), invoker(nullptr), sig_idx_to_term_idx(nullptr), sig_count(0)
    {
    }

    void *ctx;
    void (*fn)();
    void (*invoker)(Query_T *query, void (*fn)(), void *ctx);
    int32_t *sig_idx_to_term_idx; // map sig idx to term idx to support callback
                              // signature shuffle
    uint32_t sig_count;
};

// =========================================================
//
//                    ** QueryFilterCallback **
//
//  Filter callback of a query.
//
//  This is an abstract struct for exec callback that may
//  have multiple parameter signatures.
//
//  Also support shuffled callback sig without following
//  query term order.
//
// =========================================================

struct QueryFilterCallback
{
    QueryFilterCallback()
        : ctx(nullptr), fn(nullptr), invoker(nullptr), sig_idx_to_term_idx(nullptr), sig_count(0)
    {
    }

    void *ctx;
    bool (*fn)();
    void (*invoker)(Query_T *query, bool (*fn)(), MatchedArchetype &qAr, uint32_t eIdx, void *ctx);
    int32_t *sig_idx_to_term_idx; // map sig idx to term idx to support callback
                              // signature shuffle
    uint32_t sig_count;
};

constexpr const uint32_t MaxTermCount = 32;

// =========================================================
//
//                    ** QueryDesc **
//
//  Use to build query.
//
// =========================================================

struct QueryDesc
{
    QueryDesc() : eid(0), cache(false) {}

    QueryTerm terms[MaxTermCount];
    EntityId eid;
    bool cache;
};

template <typename... T>
class QueryBuilder;

// =========================================================
//
//                    ** Query **
//
//  The query itself.
//
//  Query is an internal resource. It contains intrusive ref.
//
//  This should never be exposed to public API.
//
// =========================================================

struct Query_T
{
    explicit Query_T()
        : world(nullptr), eid(ENTITY_INVALID_ID), terms(nullptr), sorted_term_indices(nullptr), term_count(0),
          is_filtered(false), result(), exec_callback(), filter_callback(),
          last_sweep_archetype(0), structure_change_count(0), is_cached(false), is_result_available(false)
    {
    }

    Query_T(Query_T &&other) noexcept
        : world(other.world), eid(other.eid), terms(other.terms), result(std::move(other.result)),
          term_count(other.term_count), is_filtered(other.is_filtered),
          sorted_term_indices(other.sorted_term_indices), last_sweep_archetype(other.last_sweep_archetype),
          filter_callback(other.filter_callback), exec_callback(other.exec_callback),
          is_cached(other.is_cached), structure_change_count(other.structure_change_count),
          is_result_available(false)
    {
        other.terms = nullptr;
        other.sorted_term_indices = nullptr;
    }

    NO_MOVE_ASSIGNMENT(Query_T);

    // void Move(Query_T &&other);

    // Query &operator=(Query &&other) noexcept
    // {
    //     eId = other.eId;
    //     world = other.world;
    //     terms = other.terms;
    //     result = std::move(other.result);
    //     execCallback = other.execCallback;
    //     termCount = other.termCount;
    //     isEntityFiltered = other.isEntityFiltered;
    //     sortedTermIdx = other.sortedTermIdx;
    //     lastSweep_archetype = other.lastSweep_archetype;
    //     entityFilterCallback = other.entityFilterCallback;
    //     isCached = other.isCached;
    //     structureChangeCount = other.structureChangeCount;
    //
    //     other.terms = nullptr;
    //     other.sortedTermIdx = nullptr;
    //
    //     return *this;
    // }

    template <typename... CallbackArgs>
    void set_execution_callback(void (*)(CallbackArgs...), void *ctx = nullptr);

    template <typename... CallbackArgs>
    void set_filter_callback(bool (*)(CallbackArgs...), void *ctx = nullptr);

    void execute();

    void init(World *world, EntityId eid = ENTITY_INVALID_ID);
    void destroy();

    void query_archetypes();

    void filter_all();
    void filter_archetype(MatchedArchetype &qAr);
    void filter_entity(MatchedArchetype &qAr, uint32_t eIdx);

    void clear_result();

    void add_if_match(Archetype *archetype);

    template <typename... CallbackArgs, size_t... I>
    void invoke_exec_callback(void (*cb)(CallbackArgs...), const QueryIter &iter,
                            MatchedArchetype &matched, uint32_t eidx, std::index_sequence<I...>);

    template <typename... CallbackArgs, size_t... I>
    bool invoke_filter_callback(bool (*cb)(CallbackArgs...), const QueryIter &iter,
                              MatchedArchetype &matched, uint32_t eidx, std::index_sequence<I...>);

    template <typename CallbackArg>
    CallbackArg get_exec_callback_arg(const QueryIter &iter, MatchedArchetype &matched, uint32_t sig_idx,
                                   uint32_t eidx);

    template <typename CallbackArg>
    CallbackArg get_filter_callback_arg(const QueryIter &iter, MatchedArchetype &matched,
                                     uint32_t sig_idx, uint32_t eidx);

    World *world;
    EntityId eid; // = EcsInvalidId if not cache
    QueryTerm *terms;
    uint8_t *sorted_term_indices;
    QueryResult result;
    QueryExecCallback exec_callback;
    QueryFilterCallback filter_callback;
    uint32_t last_sweep_archetype;
    uint32_t term_count;
    uint8_t structure_change_count;
    bool is_filtered;
    bool is_cached;
    bool is_result_available;
};

// =========================================================
//
//                    ** QueryHandle **
//
//  Public handle for query.
//
//  User will operate through this handle.
//
// =========================================================

class QueryHandle
{
private:
    Query_T *query;
public:
    NO_COPY_CTOR(QueryHandle);
    NO_COPY_ASSIGNMENT(QueryHandle);

    QueryHandle(QueryHandle &&other) noexcept
    {
        query = other.query;
        other.query = nullptr;
    }

    QueryHandle &operator=(QueryHandle &&other) noexcept
    {
        query = other.query;
        other.query = nullptr;
        return *this;
    }

    void execute();

private:
    template <typename Derived, typename Handle>
    friend class QueryCallBackBuilderBase;
    explicit QueryHandle(Query_T *query) : query(query) { ENGINE_ASSERT(query, "Query is null!"); }

    void destroy();
};

// =========================================================
//
//                    ** SystemHandle **
//
//  Public handle for system.
//
//  User will operate through this handle.
//
// =========================================================

using SystemHandle = QueryHandle;

// =========================================================
//
//                    ** QueryBuilderBase **
//
//  Use to build QueryDesc
//
// =========================================================

template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
class QueryBuilderBase
{
protected:
    QueryBuilderBase(World *world)
        : world(world), cur_term_idx(0), desc(), first_term(true), structure_change_count(0)
    {
        static_assert(!std::is_reference_v<Handle> && !std::is_const_v<Handle>);
        static_assert(((!std::is_reference_v<T> && !std::is_const_v<T>) && ...),
                      "Query terms must not be ref or const!");
        ENGINE_ASSERT(world, "World must not be null!");
        (term<T>(), ...);
    }

public:
    template <typename U>
    Derived &with(TermAccess term_access = TermAccess::READ);

    template <typename U>
    Derived &without();

    Derived &with(EntityId tid, TermAccess term_access = TermAccess::READ);

    Derived &without(EntityId tid);

    Derived &with(EntityId relation_id, EntityId target_eid, TermAccess term_access = TermAccess::READ);

    Derived &without(EntityId relation_id, EntityId target_eid);

    template <typename U>
    Derived &up(EntityId target_eid, TermAccess term_access = TermAccess::READ);

    template <typename U>
    Derived &cascade(TermAccess term_access = TermAccess::READ);

    Derived &up(EntityId relation_id, EntityId target_eid, TermAccess term_access = TermAccess::READ);
    Derived &cascade(EntityId relation_id, EntityId target_eid, TermAccess term_access = TermAccess::READ);

    template <typename U>
    Derived &structure_change();

    template <typename... CallbackArgs>
    CallbackBuilder filter(bool (*)(CallbackArgs...), void *ctx = nullptr);

    template <typename... CallbackArgs>
    Handle each(void (*)(CallbackArgs...), void *ctx = nullptr, const char * name = nullptr);

protected:
    Derived &self() { return *SCAST(Derived *, this); }

    Query_T *create_query();

    Derived &term(EntityId eid);
    Derived &term(EntityId first, EntityId second);

    template <typename U>
    Derived &term();

    Derived &access(TermAccess term_access);
    Derived &through(TraverseMethod method);

    // QueryBuilder<T...> &TraveseTarget(EntityId targetId);

    Derived &traverse(EntityId relation_id, EntityId target_eid);
    Derived &traverse_any(EntityId relation_id);

    template <typename U>
    Derived &traverse(EntityId target);

    template <typename U>
    Derived &traverse_any();

    Derived &op(TermOp op);

protected:
    World *world;
    QueryDesc desc;
    QueryTerm cur_term;
    uint32_t cur_term_idx;
    uint8_t structure_change_count;
    bool first_term;
};

// =========================================================
//
//                ** QueryCallBackBuilderBase **
//
// =========================================================

template <typename Derived, typename Handle>
class QueryCallBackBuilderBase
{
public:
    template <typename... CallbackArgs>
    QueryCallBackBuilderBase<Derived, Handle> &filter(bool (*)(CallbackArgs...),
                                                      void *ctx = nullptr);

    template <typename... CallbackArgs>
    Handle each(void (*)(CallbackArgs...), void *ctx = nullptr, const char *name = nullptr);

protected:
    QueryCallBackBuilderBase(Query_T *query) : query(query)
    {
        ENGINE_ASSERT(query, "Query is null");
    }

protected:
    Query_T *query;
};

// =========================================================
//
//               ** QueryCallbackBuilder **
//
// =========================================================

class QueryCallbackBuilder : public QueryCallBackBuilderBase<QueryCallbackBuilder, QueryHandle>
{
public:
    QueryCallbackBuilder &cache()
    {
        query->is_cached = true;
        return *this;
    }

protected:
    template <typename Derived, typename Handle>
    friend class QueryCallBackBuilderBase;
    void create_cache_entity(const char *name);

private:
    template <typename... T>
    friend class QueryBuilder;

    template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
    friend class QueryBuilderBase;

    QueryCallbackBuilder(Query_T *query)
        : QueryCallBackBuilderBase<QueryCallbackBuilder, QueryHandle>(query)
    {
    }
};

// =========================================================
//
//              ** SystemCallbackBuilder **
//
// =========================================================

class SystemCallbackBuilder : public QueryCallBackBuilderBase<SystemCallbackBuilder, SystemHandle>
{
public:
    SystemCallbackBuilder &phase(EntityId phase_id)
    {
        phase_id_ = phase_id;
        return *this;
    }

protected:
    template <typename Derived, typename Handle>
    friend class QueryCallBackBuilderBase;
    void create_cache_entity(const char *name);

private:
    template <typename... T>
    friend class SystemBuilder;

    template <typename Handle, typename Derived, typename CallbackBuilder, typename... T>
    friend class QueryBuilderBase;

    SystemCallbackBuilder(Query_T *query)
        : phase_id_(ENTITY_INVALID_ID), QueryCallBackBuilderBase<SystemCallbackBuilder, SystemHandle>(query)
    {
        ENGINE_ASSERT(query, "Query is null!");
        query->is_cached = true;
    }

    EntityId phase_id_;
};

// =========================================================
//
//                  ** QueryBuilder **
//
//  Inherit QueryBuilderBase.
//
//  Return QueryHandle as final step.
//
// =========================================================

template <typename... T>
class QueryBuilder
    : public QueryBuilderBase<QueryHandle, QueryBuilder<T...>, QueryCallbackBuilder, T...>
{
public:
    QueryCallbackBuilder cache(EntityId eId);

    QueryBuilder(World *world)
        : ECS::QueryBuilderBase<QueryHandle, QueryBuilder<T...>, QueryCallbackBuilder, T...>(world)
    {
    }
};

// =========================================================
//
//                  ** SystemBuilder **
//
//  Inherit QueryBuilderBase.
//
//  Return SystemHandle as final step.
//
// =========================================================

template <typename... T>
class SystemBuilder
    : public QueryBuilderBase<SystemHandle, SystemBuilder<T...>, SystemCallbackBuilder, T...>
{
public:
    SystemCallbackBuilder phase(EntityId eId);

    SystemBuilder(World *world)
        : ECS::QueryBuilderBase<SystemHandle, SystemBuilder<T...>, SystemCallbackBuilder, T...>(world)
    {
    }
};
} // namespace VoidEngine::ECS
