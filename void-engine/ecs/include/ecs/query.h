#pragma once
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "entity.h"
#include "internal_component.h"
#include <cassert>
#include <cstdint>
#include <type_traits>

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
    SELF_UP,
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
        : cId(0), travRelation(0), travTarget(0), trav(SELF), op(HAS),
          behavior(READ_WRITE), fieldId(0)
    {
    }

    EntityId cId;
    EntityId travRelation;
    EntityId travTarget;
    TraverseMethod trav;
    TermOp op;
    TermBehavior behavior;
    uint16_t fieldId;
};

constexpr uint16_t InlineArrayOptimizationCount = 8;

struct MatchedArchetype
{
    MatchedArchetype() = default;

    MatchedArchetype(const Archetype *archetype)
        : archetype(archetype), largeEntityMask(nullptr),
          largeMatchedColumns(nullptr)
    {
    }

    MatchedArchetype(MatchedArchetype &&other) noexcept
    {
        archetype = other.archetype;
        largeMatchedColumns = other.largeMatchedColumns;
        std::memcpy(smallMatchedColumns, other.smallMatchedColumns,
                    sizeof(int32_t) * InlineArrayOptimizationCount);
        largeEntityMask = other.largeEntityMask;

        other.archetype = nullptr;
        other.largeEntityMask = nullptr;
    }

    MatchedArchetype &operator=(MatchedArchetype &&other) noexcept
    {
        archetype = other.archetype;
        largeMatchedColumns = other.largeMatchedColumns;
        std::memcpy(smallMatchedColumns, other.smallMatchedColumns,
                    sizeof(int32_t) * InlineArrayOptimizationCount);
        largeEntityMask = other.largeEntityMask;

        other.archetype = nullptr;
        other.largeEntityMask = nullptr;

        return *this;
    }

    void SetMatchedColumns(WorldAllocator &wAllocator, int32_t *mappedCols,
                           uint32_t count);

    const Archetype *archetype;
    int32_t *largeMatchedColumns;
    int32_t smallMatchedColumns[InlineArrayOptimizationCount];

    // NOTE: if there are ecs operation like add or delete, bitmask will be
    // invalidated. Try to avoid these expensive filtering as much as possible
    uint32_t smallEntityMask[InlineArrayOptimizationCount];
    uint32_t *largeEntityMask;

    void Delete(WorldAllocator &wAllocator, uint32_t callbackSigCount);
};

struct QueryResult
{
    QueryResult() : largeMatchedArchetypes(nullptr), count(0), capacity(0) {}

    QueryResult(QueryResult &&other) noexcept
    {
        count = other.count;
        capacity = other.capacity;
        largeMatchedArchetypes = other.largeMatchedArchetypes;

        uint32_t smallCount = (count <= InlineArrayOptimizationCount)
                                  ? count
                                  : InlineArrayOptimizationCount;
        for (size_t idx = 0; idx < smallCount; ++idx)
        {
            smallMatchedArchetypes[idx] =
                std::move(other.smallMatchedArchetypes[idx]);
        }

        other.largeMatchedArchetypes = nullptr;
    }

    QueryResult &operator=(QueryResult &&other) noexcept
    {
        count = other.count;
        capacity = other.capacity;
        largeMatchedArchetypes = other.largeMatchedArchetypes;

        uint32_t smallCount = (count <= InlineArrayOptimizationCount)
                                  ? count
                                  : InlineArrayOptimizationCount;
        for (size_t idx = 0; idx < smallCount; ++idx)
        {
            smallMatchedArchetypes[idx] =
                std::move(other.smallMatchedArchetypes[idx]);
        }

        other.largeMatchedArchetypes = nullptr;

        return *this;
    }

    MatchedArchetype &operator[](size_t idx)
    {
        if (idx < count)
        {
            if (idx < InlineArrayOptimizationCount)
            {
                return smallMatchedArchetypes[idx];
            }
            else
            {
                assert(largeMatchedArchetypes);
                return largeMatchedArchetypes[idx -
                                              InlineArrayOptimizationCount];
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

    MatchedArchetype smallMatchedArchetypes[InlineArrayOptimizationCount];
    MatchedArchetype *largeMatchedArchetypes;

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
constexpr bool at_most_one_entity =
    ((std::is_same_v<CallbackArgs, Entity> ? 1 : 0) + ...) <= 1;

template <typename... CallbackArgs>
constexpr bool at_most_one_query_iter =
    ((std::is_same_v<CallbackArgs, QueryResult> ? 1 : 0) + ...) <= 1;


class Query;

// Map callback signature to query term index
constexpr int32_t QueryIterIndex = -1;
constexpr int32_t EntityIndex = -2;
constexpr int32_t InvalidIndex = -3;

struct QueryCallback
{
    void *ctx;
    void (*fn)();
    void (*invoker)(Query *query, void (*fn)(), void *ctx);
    int32_t *mappedSig;
    uint32_t sigCount;

    // template <typename... CallbackArgs>
    // static QueryCallback CreateCallback(Query query, void
    // (*fn)(CallbackArgs...));
};

struct QueryDesc
{
    QueryDesc() : eId(0), cache(false) {}

    QueryTerm terms[32];
    EntityId eId;
    bool cache;
};

template <typename... T>
class QueryBuilder;

class Query
{
public:
    Query(Query &&other) noexcept
    {
        m_world = other.m_world;
        m_eId = other.m_eId;
        m_terms = other.m_terms;
        m_result = std::move(other.m_result);
        m_callback = other.m_callback;
        m_termCount = other.m_termCount;
        m_isEntityFiltered = other.m_isEntityFiltered;

        other.m_terms = nullptr;
    }

    Query &operator=(Query &&other) noexcept
    {
        m_world = other.m_world;
        m_eId = other.m_eId;
        m_terms = other.m_terms;
        m_result = std::move(other.m_result);
        m_callback = other.m_callback;
        m_termCount = other.m_termCount;
        m_isEntityFiltered = other.m_isEntityFiltered;

        other.m_terms = nullptr;

        return *this;
    }


    template <typename... CallbackArgs>
    void Each(void (*)(CallbackArgs...), void *ctx = nullptr);

    void Execute();

    void Destroy();

    template<typename... CallbackArgs, size_t... I>
    static void InvokeCallback(
        Query* query,
        void (*cb)(CallbackArgs...),
        QueryIter& iter,
        Archetype* archetype,
        uint32_t eIdx,
        std::index_sequence<I...>)
    {
        cb(query->GetArg<CallbackArgs>(iter, archetype, I, eIdx)...);
    }

private:
    template <typename... T>
    friend class QueryBuilder;

    Query(World *world, EntityId eId = 0) : m_world(world), m_eId(eId) {}

    void Filter();

    template <typename CallbackArg>
    CallbackArg GetArg(QueryIter iter, Archetype *archetype, uint32_t sigIdx,
                        uint32_t eIdx);

private:
    World *m_world;
    EntityId m_eId; // = 0 if not cache
    QueryTerm *m_terms;
    QueryResult m_result;
    QueryCallback m_callback;
    uint32_t m_termCount;
    bool m_isEntityFiltered;
};

template <typename... T>
class QueryBuilder
{
public:
    QueryBuilder(World *world)
        : m_world(world), m_currTermIdx(0), m_desc(), m_firstTerm(true)
    {
        assert(m_world);
        (Term<T>(), ...);
    }

    QueryBuilder<T...> &Term(EntityId id);

    QueryBuilder<T...> &Term(EntityId first, EntityId second);

    template <typename U>
    QueryBuilder<T...> &Term();

    QueryBuilder<T...> &Traverse(TraverseMethod method);

    QueryBuilder<T...> &TraveseTarget(EntityId targetId);

    QueryBuilder<T...> &TraveseTarget(EntityId first, EntityId second);

    QueryBuilder<T...> &Op(TermOp op);

    QueryBuilder<T...> &Cache(EntityId cacheId);

    QueryBuilder<T...> &Scope();

    template <typename U>
    QueryBuilder<T...> &With();

    template <typename U>
    QueryBuilder<T...> &Without();

    QueryBuilder<T...> &With();

    QueryBuilder<T...> &Without();

    QueryBuilder<T...> &SelfUp(EntityId target);

    QueryBuilder<T...> &SelfUp(EntityId first, EntityId second);

    QueryBuilder<T...> &Up(EntityId target);

    QueryBuilder<T...> &Up(EntityId first, EntityId second);

    QueryBuilder<T...> &Cascade(EntityId target);

    QueryBuilder<T...> &Cascade(EntityId first, EntityId second);

    template <typename U>
    QueryBuilder<T...> &Modify();

    Query *Build();

private:
    World *m_world;
    QueryDesc m_desc;
    QueryTerm m_currTerm;
    uint32_t m_currTermIdx;
    bool m_firstTerm;
};
} // namespace ECS
