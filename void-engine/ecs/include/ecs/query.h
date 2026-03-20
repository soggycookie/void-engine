#pragma once
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "entity.h"
#include "internal_component.h"

namespace ECS
{
struct ArchetypeLinkedList
{
    Archetype *archetype;
    ArchetypeLinkedList *next;

    static ArchetypeLinkedList *Init(WorldAllocator &wAllocator)
    {
        ArchetypeLinkedList *all =
            PTR_CAST(wAllocator.Calloc(sizeof(ArchetypeLinkedList)),
                     ArchetypeLinkedList);

        return all;
    }

    static void Free(WorldAllocator &wAllocator, void *addr)
    {
        wAllocator.Free(sizeof(ArchetypeLinkedList), addr);
    }
};

struct QueryIterator
{
    World *world;
    Archetype *archetype;
    uint32_t row;
    // double deltaTime;

    Entity GetEntity()
    {
        EntityId id = archetype->entities[row];

        return Entity(id, world);
    }

    template <typename Component>
    Component &Get()
    {
        uint32_t colIdx =
            archetype->componentSet.Search(ComponentTypeId<EntityId>::Id());

        assert(colIdx != -1);

        Column &col = archetype->columns[colIdx];
        TypeInfo &ti = *col.typeInfo;
        void *comData = OFFSET(col.data, ti.size * row);

        return *PTR_CAST(comData, Component);
    }
};

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
    OPTIONAL
};

enum TermBehavior : uint16_t
{
    READ_WRITE,
    STRUCTURE_CHANGE
};

struct QueryTerm
{
    QueryTerm()
        : cId(EcsInvalidId), travRelation(EcsInvalidId),
          travTarget(EcsInvalidId), validTravTarget(EcsInvalidId),
          travMethod(SELF), op(HAS), behavior(READ_WRITE), fieldId(0)
    {
    }

    EntityId cId;
    EntityId travRelation;
    EntityId travTarget;
    EntityId validTravTarget;
    TraverseMethod travMethod;
    TermOp op;
    TermBehavior behavior;
    uint16_t fieldId;
};

constexpr uint16_t InlineArrayOptimizationCount = 8;

struct MatchedArchetype
{
    MatchedArchetype() : hi_matchedColumnIdx(nullptr), hi_entityMask(nullptr) {}

    MatchedArchetype(const Archetype *archetype)
        : archetype(archetype), hi_entityMask(nullptr),
          hi_matchedColumnIdx(nullptr)
    {
    }

    MatchedArchetype(MatchedArchetype &&other) noexcept
    {
        archetype = other.archetype;
        hi_matchedColumnIdx = other.hi_matchedColumnIdx;
        std::memcpy(lo_matchedColumnIdx, other.lo_matchedColumnIdx,
                    sizeof(int32_t) * InlineArrayOptimizationCount);
        hi_entityMask = other.hi_entityMask;
        std::memcpy(lo_entityMask, other.lo_entityMask,
                    sizeof(int32_t) * InlineArrayOptimizationCount);

        other.archetype = nullptr;
        other.hi_entityMask = nullptr;
    }

    MatchedArchetype &operator=(MatchedArchetype &&other) noexcept
    {
        archetype = other.archetype;
        hi_matchedColumnIdx = other.hi_matchedColumnIdx;
        std::memcpy(lo_matchedColumnIdx, other.lo_matchedColumnIdx,
                    sizeof(int32_t) * InlineArrayOptimizationCount);
        hi_entityMask = other.hi_entityMask;
        std::memcpy(lo_entityMask, other.lo_entityMask,
                    sizeof(int32_t) * InlineArrayOptimizationCount);

        other.archetype = nullptr;
        other.hi_entityMask = nullptr;

        return *this;
    }

    void SetMatchedColumnIdx(WorldAllocator &wAllocator, int32_t *mappedCols,
                             uint32_t count);

    int32_t GetColumnIdx(uint32_t termIdx) const;

    void Delete(WorldAllocator &wAllocator, uint32_t callbackSigCount);

    const Archetype *archetype;
    int32_t *hi_matchedColumnIdx; // map term index to column index
    int32_t lo_matchedColumnIdx[InlineArrayOptimizationCount];

    // NOTE: if there are ecs operation like add or delete, bitmask will be
    // invalidated. Try to avoid these expensive filtering as much as possible
    uint32_t lo_entityMask[InlineArrayOptimizationCount];
    uint32_t *hi_entityMask;
};

struct QueryResult
{
    QueryResult() : hi_matchedArchetypes(nullptr), count(0), capacity(0) {}

    QueryResult(QueryResult &&other) noexcept
    {
        count = other.count;
        capacity = other.capacity;

        int32_t countDiff = count - InlineArrayOptimizationCount;
        if (countDiff <= 0)
        {
            for (size_t idx = 0; idx < count; ++idx)
            {
                lo_matchedArchetypes[idx] =
                    std::move(other.lo_matchedArchetypes[idx]);
            }
        }
        else
        {
            assert(other.hi_matchedArchetypes);
            hi_matchedArchetypes = other.hi_matchedArchetypes;
        }
        other.hi_matchedArchetypes = nullptr;
    }

    QueryResult &operator=(QueryResult &&other) noexcept
    {
        count = other.count;
        capacity = other.capacity;

        int32_t countDiff = count - InlineArrayOptimizationCount;
        if (countDiff <= 0)
        {
            for (size_t idx = 0; idx < count; ++idx)
            {
                lo_matchedArchetypes[idx] =
                    std::move(other.lo_matchedArchetypes[idx]);
            }
        }
        else
        {
            assert(other.hi_matchedArchetypes);
            hi_matchedArchetypes = other.hi_matchedArchetypes;
        }
        other.hi_matchedArchetypes = nullptr;

        return *this;
    }

    MatchedArchetype &operator[](size_t idx)
    {
        if (idx < count)
        {
            if (idx < InlineArrayOptimizationCount)
            {
                return lo_matchedArchetypes[idx];
            }
            else
            {
                assert(hi_matchedArchetypes);
                return hi_matchedArchetypes[idx - InlineArrayOptimizationCount];
            }
        }
        assert(0);

#ifdef __clang__
        __builtin_unreachable();
#elif defined(_MSC_VER)
        __assume(false);
#endif
    }

    void Add(WorldAllocator &wAllocator, MatchedArchetype &&matched);

    void Delete(WorldAllocator &wAllocator, uint32_t callbackSigCount);

    MatchedArchetype lo_matchedArchetypes[InlineArrayOptimizationCount];
    MatchedArchetype *hi_matchedArchetypes;

    uint32_t count;
    uint32_t capacity;
};

struct QueryIter
{
    QueryIter(World *world, EntityId eId = 0, void *ctx = nullptr,
              double deltaTime = 0)
        : world(world), ctx(ctx), eId(eId), deltaTime(deltaTime)
    {
    }

    World *world;
    void *ctx;
    EntityId eId;
    double deltaTime;

    Entity GetEntity() { return Entity(eId, world); }
};

template <typename... CallbackArgs>
constexpr bool at_most_one_query_iter =
    ((std::is_same_v<CallbackArgs, const QueryResult &> ? 1 : 0) + ...) <= 1;

template <typename First, typename... Rest>
constexpr bool is_first_arg_query_iter =
    std::is_same_v<First, const QueryIter &>;

class Query;

// Map callback signature to query term index
constexpr int32_t QueryIterIndex = -1;
constexpr int32_t EntityIndex = -2;
constexpr int32_t InvalidIndex = -3;

struct QueryCallback
{
    QueryCallback()
        : ctx(nullptr), fn(nullptr), invoker(nullptr), sigIdxToTermIdx(nullptr),
          sigCount(0)
    {
    }

    void *ctx;
    void (*fn)();
    void (*invoker)(Query *query, void (*fn)(), void *ctx);
    int32_t *sigIdxToTermIdx; // map sig idx to term idx to support callback
                              // signature shuffle
    uint32_t sigCount;
};

constexpr const uint32_t MaxTermCount = 32;

struct QueryDesc
{
    QueryDesc() : eId(0), cache(false) {}

    QueryTerm terms[MaxTermCount];
    EntityId eId;
    bool cache;
};

template <typename... T>
class QueryBuilder;

struct Query
{
    Query(World *world, EntityId eId = 0)
        : world(world), eId(eId), terms(nullptr), sortedTermIdx(nullptr),
          termCount(0), isEntityFiltered(false), result(), callback()
    {
    }

    Query(Query &&other) noexcept
    {
        world = other.world;
        eId = other.eId;
        terms = other.terms;
        result = std::move(other.result);
        callback = other.callback;
        termCount = other.termCount;
        isEntityFiltered = other.isEntityFiltered;
        sortedTermIdx = other.sortedTermIdx;

        other.terms = nullptr;
        other.sortedTermIdx = nullptr;
    }

    Query &operator=(Query &&other) noexcept
    {
        world = other.world;
        eId = other.eId;
        terms = other.terms;
        result = std::move(other.result);
        callback = other.callback;
        termCount = other.termCount;
        isEntityFiltered = other.isEntityFiltered;
        sortedTermIdx = other.sortedTermIdx;

        other.terms = nullptr;
        other.sortedTermIdx = nullptr;

        return *this;
    }

    template <typename... CallbackArgs>
    void Each(void (*)(CallbackArgs...), void *ctx = nullptr);

    void Execute();

    void Destroy();

    void Filter();

    template <typename... CallbackArgs, size_t... I>
    static void InvokeCallback(Query *query, void (*cb)(CallbackArgs...),
                               QueryIter &iter, const MatchedArchetype &matched,
                               uint32_t eIdx, std::index_sequence<I...>);

    template <typename CallbackArg>
    CallbackArg GetArg(const QueryIter &iter, const MatchedArchetype &matched,
                       uint32_t sigIdx, uint32_t eIdx);

    World *world;
    EntityId eId; // = EcsInvalidId if not cache
    QueryTerm *terms;
    uint8_t *sortedTermIdx;
    QueryResult result;
    QueryCallback callback;
    uint32_t termCount;
    bool isEntityFiltered;
};

class QueryHandle
{
public:
    ~QueryHandle()
    {
        if (m_eId == EcsInvalidId)
        {
            Destroy();
        }
    }

    QueryHandle(QueryHandle &&other) noexcept
    {
        m_eId = other.m_eId;
        m_query = other.m_query;

        other.m_query = nullptr;
        other.m_eId = 0;
    }

    QueryHandle &operator=(QueryHandle &&other) noexcept
    {
        m_eId = other.m_eId;
        m_query = other.m_query;

        other.m_query = nullptr;
        other.m_eId = 0;

        return *this;
    }

    EntityId GetId() const { return m_eId; }

    void Execute();

    // No need to call this manually unless you use cache query
    // Ad-hoc query handle will call this when go out of scope (RAII)
    void Destroy();

private:
    template <typename... T>
    friend class QueryBuilder;

    // ad-hoc
    QueryHandle(EntityId eId, Query *query) : m_eId(eId), m_query(query)
    {
        assert(m_query);
    }

private:
    EntityId m_eId;
    Query *m_query;
};

template <typename... T>
class QueryBuilder
{
public:
    QueryBuilder(World *world)
        : m_world(world), m_currTermIdx(0), m_desc(), m_firstTerm(true)
    {
        static_assert(((!std::is_reference_v<T> && !std::is_const_v<T>) && ...),
                      "Query terms must not be ref or const!");
        assert(m_world);
        (Term<T>(), ...);
    }

    QueryBuilder<T...> &Term(EntityId id);

    QueryBuilder<T...> &Term(EntityId first, EntityId second);

    template <typename U>
    QueryBuilder<T...> &Term();

    QueryBuilder<T...> &Through(TraverseMethod method);

    // QueryBuilder<T...> &TraveseTarget(EntityId targetId);

    QueryBuilder<T...> &Traverse(EntityId relation, EntityId target);

    QueryBuilder<T...> &TraverseAny(EntityId relation);

    template <typename U>
    QueryBuilder<T...> &Traverse(EntityId target);

    template <typename U>
    QueryBuilder<T...> &TraverseAny();

    QueryBuilder<T...> &Op(TermOp op);

    QueryBuilder<T...> &Cache(EntityId cacheId = 0);

    QueryBuilder<T...> &Scope();

    template <typename U>
    QueryBuilder<T...> &With();

    template <typename U>
    QueryBuilder<T...> &Without();

    // QueryBuilder<T...> &With();
    //
    // QueryBuilder<T...> &Without();

    // QueryBuilder<T...> &SelfUp(EntityId target);
    //
    // QueryBuilder<T...> &SelfUp(EntityId first, EntityId second);

    // QueryBuilder<T...> &Up(EntityId target);

    template <typename U>
    QueryBuilder<T...> &Up(EntityId target);

    template <typename U>
    QueryBuilder<T...> &Cascade();

    QueryBuilder<T...> &Up(EntityId relation, EntityId target);

    QueryBuilder<T...> &Cascade(EntityId relation);

    template <typename U>
    QueryBuilder<T...> &Modify();

    template <typename... CallbackArgs>
    QueryHandle Each(void (*)(CallbackArgs...), void *ctx = nullptr);

private:
    World *m_world;
    QueryDesc m_desc;
    QueryTerm m_currTerm;
    uint32_t m_currTermIdx;
    bool m_firstTerm;
};
} // namespace ECS
