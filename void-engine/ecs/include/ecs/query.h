#pragma once
#include "ds/world_allocator.h"
#include "ecs_type.h"
#include "entity.h"
#include "internal_component.h"
#include "type_info.h"
#include <cstdint>
#include <type_traits>

namespace ECS
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

    struct QueryArchetype
    {
        QueryArchetype()
            : hi_matchedColumnIdx(nullptr), hi_entityMask(nullptr), maskCount(0),
            maskCapacity(0), lo_entityMask{0}, lo_matchedColumnIdx{0}
        {
        }

        QueryArchetype(const Archetype* archetype)
            : archetype(archetype), hi_entityMask(nullptr), maskCapacity(0),
            hi_matchedColumnIdx(nullptr), maskCount(0), lo_entityMask{0},
            lo_matchedColumnIdx{0}
        {
        }

        QueryArchetype(QueryArchetype&& other) noexcept
        {
            archetype = other.archetype;
            hi_matchedColumnIdx = other.hi_matchedColumnIdx;
            std::memcpy(lo_matchedColumnIdx, other.lo_matchedColumnIdx,
                        sizeof(int32_t) * InlineArrayOptimizationCount);
            hi_entityMask = other.hi_entityMask;
            std::memcpy(lo_entityMask, other.lo_entityMask,
                        sizeof(int32_t) * InlineArrayOptimizationCount);
            maskCount = other.maskCount;
            maskCapacity = other.maskCapacity;
            other.archetype = nullptr;
            other.hi_entityMask = nullptr;
        }

        QueryArchetype& operator=(QueryArchetype&& other) noexcept
        {
            archetype = other.archetype;
            hi_matchedColumnIdx = other.hi_matchedColumnIdx;
            std::memcpy(lo_matchedColumnIdx, other.lo_matchedColumnIdx,
                        sizeof(int32_t) * InlineArrayOptimizationCount);
            hi_entityMask = other.hi_entityMask;
            std::memcpy(lo_entityMask, other.lo_entityMask,
                        sizeof(int32_t) * InlineArrayOptimizationCount);
            maskCount = other.maskCount;
            maskCapacity = other.maskCapacity;
            other.archetype = nullptr;
            other.hi_entityMask = nullptr;

            return *this;
        }

        void SetMatchedColumnIdx(WorldAllocator& wAllocator, int32_t* mappedCols,
                                 uint32_t count);

        int32_t GetColumnIdx(uint32_t termIdx) const;

        void Delete(WorldAllocator& wAllocator, uint32_t callbackSigCount);

        void AllocateMask(WorldAllocator& wAllocator);
        void SetMask(uint32_t eIdx, bool bit);
        bool GetMask(uint32_t eIdx);

        uint32_t& Mask(uint32_t eIdx);

        static constexpr const uint32_t EntityPerMask = 32;

        const Archetype* archetype;
        int32_t* hi_matchedColumnIdx; // map term index to column index
        int32_t lo_matchedColumnIdx[InlineArrayOptimizationCount];

        // NOTE: if there are ecs operation like add or delete, bitmask will be
        // invalidated. Try to avoid these expensive filtering as much as possible
        uint32_t lo_entityMask[InlineArrayOptimizationCount];
        uint32_t* hi_entityMask;
        uint32_t maskCount;
        uint32_t maskCapacity;
    };

    struct QueryResult
    {
        QueryResult() : hi_queryArchetypes(nullptr), count(0), capacity(0) {}

        QueryResult(QueryResult&& other) noexcept
        {
            count = other.count;
            capacity = other.capacity;

            int32_t countDiff = count - InlineArrayOptimizationCount;
            if(countDiff <= 0)
            {
                for(size_t idx = 0; idx < count; ++idx)
                {
                    lo_queryArchetypes[idx] =
                        std::move(other.lo_queryArchetypes[idx]);
                }
            }
            else
            {
                assert(other.hi_queryArchetypes);
                hi_queryArchetypes = other.hi_queryArchetypes;
            }
            other.hi_queryArchetypes = nullptr;
        }

        QueryResult& operator=(QueryResult&& other) noexcept
        {
            count = other.count;
            capacity = other.capacity;

            int32_t countDiff = count - InlineArrayOptimizationCount;
            if(countDiff <= 0)
            {
                for(size_t idx = 0; idx < count; ++idx)
                {
                    lo_queryArchetypes[idx] =
                        std::move(other.lo_queryArchetypes[idx]);
                }
            }
            else
            {
                assert(other.hi_queryArchetypes);
                hi_queryArchetypes = other.hi_queryArchetypes;
            }
            other.hi_queryArchetypes = nullptr;

            return *this;
        }

        QueryArchetype& operator[](size_t idx)
        {
            if(idx < count)
            {
                if(idx < InlineArrayOptimizationCount)
                {
                    return lo_queryArchetypes[idx];
                }
                else
                {
                    assert(hi_queryArchetypes);
                    return hi_queryArchetypes[idx - InlineArrayOptimizationCount];
                }
            }
            assert(0);

#ifdef __clang__
            __builtin_unreachable();
#elif defined(_MSC_VER)
            __assume(false);
#endif
        }

        void Add(WorldAllocator& wAllocator, QueryArchetype&& matched);

        void Delete(WorldAllocator& wAllocator, uint32_t callbackSigCount);

        QueryArchetype lo_queryArchetypes[InlineArrayOptimizationCount];
        QueryArchetype* hi_queryArchetypes;

        uint32_t count;
        uint32_t capacity;
    };

    struct QueryIter
    {
        QueryIter(World* world, EntityId eId = 0, void* ctx = nullptr,
                  double deltaTime = 0)
            : world(world), ctx(ctx), eId(eId), deltaTime(deltaTime)
        {
        }

        World* world;
        void* ctx;
        EntityId eId;
        double deltaTime;

        Entity GetEntity() { return Entity(eId, world); }
    };

    template <typename... CallbackArgs>
    constexpr bool has_one_query_iter_v =
        ((std::is_same_v<std::decay_t<CallbackArgs>, QueryResult> ? 1 : 0) + ...) ==
        1;

    template <typename First, typename... Rest>
    constexpr bool is_first_arg_query_iter_v =
        std::is_same_v<First, const QueryIter&>;

    class Query;

    // Map callmatched.matchedColummatched.matchedColumns signature to query term
    // index
    constexpr int32_t QueryIterIndex = -1;
    constexpr int32_t InvalidIndex = -2;

    struct QueryCallback
    {
        QueryCallback()
            : ctx(nullptr), fn(nullptr), invoker(nullptr), sigIdxToTermIdx(nullptr),
            sigCount(0)
        {
        }

        void* ctx;
        void (*fn)();
        void (*invoker)(Query* query, void (*fn)(), void* ctx);
        int32_t* sigIdxToTermIdx; // map sig idx to term idx to support callback
                                  // signature shuffle
        uint32_t sigCount;
    };

    struct QueryFilterCallback
    {
        QueryFilterCallback()
            : ctx(nullptr), fn(nullptr), invoker(nullptr), sigIdxToTermIdx(nullptr),
            sigCount(0)
        {
        }

        void* ctx;
        bool (*fn)();
        void (*invoker)(Query* query, bool (*fn)(), QueryArchetype& qAr,
                        uint32_t eIdx, void* ctx);
        int32_t* sigIdxToTermIdx; // map sig idx to term idx to support callback
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

    struct MatchedArchetype
    {
        int* matchedColumns;
        bool matched;
    };

    struct Query
    {
        Query(World* world, EntityId eId = 0)
            : world(world), eId(eId), terms(nullptr), sortedTermIdx(nullptr),
            termCount(0), isEntityFiltered(false), result(), execCallback(),
            entityFilterCallback(), lastSweep_archetype(0), isCached(false)
        {
        }

        Query(Query&& other) noexcept
        {
            world = other.world;
            eId = other.eId;
            terms = other.terms;
            result = std::move(other.result);
            execCallback = other.execCallback;
            termCount = other.termCount;
            isEntityFiltered = other.isEntityFiltered;
            sortedTermIdx = other.sortedTermIdx;
            lastSweep_archetype = other.lastSweep_archetype;
            entityFilterCallback = other.entityFilterCallback;
            isCached = other.isCached;

            other.terms = nullptr;
            other.sortedTermIdx = nullptr;
        }

        Query& operator=(Query&& other) noexcept
        {
            world = other.world;
            eId = other.eId;
            terms = other.terms;
            result = std::move(other.result);
            execCallback = other.execCallback;
            termCount = other.termCount;
            isEntityFiltered = other.isEntityFiltered;
            sortedTermIdx = other.sortedTermIdx;
            lastSweep_archetype = other.lastSweep_archetype;
            entityFilterCallback = other.entityFilterCallback;
            isCached = other.isCached;

            other.terms = nullptr;
            other.sortedTermIdx = nullptr;

            return *this;
        }

        template <typename... CallbackArgs>
        void ExecutionCallback(void (*)(CallbackArgs...), void* ctx = nullptr);

        template <typename... CallbackArgs>
        void FilterCallback(bool (*)(CallbackArgs...), void* ctx = nullptr);

        void Execute();

        void Destroy();

        void FilterArchetype();

        void FilterResultEntity();

        void FilterEntity(QueryArchetype& qAr);

        void FilterEntity(QueryArchetype& qAr, uint32_t eIdx);

        MatchedArchetype IsMatch(Archetype* archeytype);

        template <typename... CallbackArgs, size_t... I>
        void InvokeExecCallback(void (*cb)(CallbackArgs...), const QueryIter& iter,
                                QueryArchetype& matched, uint32_t eIdx,
                                std::index_sequence<I...>);

        template <typename... CallbackArgs, size_t... I>
        bool InvokeFilterCallback(bool (*cb)(CallbackArgs...),
                                  const QueryIter& iter, QueryArchetype& matched,
                                  uint32_t eIdx, std::index_sequence<I...>);

        template <typename CallbackArg>
        CallbackArg GetExecCallbackArg(const QueryIter& iter, QueryArchetype& matched,
                                       uint32_t sigIdx, uint32_t eIdx);

        template <typename CallbackArg>
        CallbackArg GetFilterCallbackArg(const QueryIter& iter, QueryArchetype& matched,
                                         uint32_t sigIdx, uint32_t eIdx);

        World* world;
        EntityId eId; // = EcsInvalidId if not cache
        QueryTerm* terms;
        uint8_t* sortedTermIdx;
        QueryResult result;
        QueryCallback execCallback;
        QueryFilterCallback entityFilterCallback;
        uint32_t lastSweep_archetype;
        uint32_t termCount;
        bool isEntityFiltered;
        bool isCached;
    };

    class QueryHandle
    {
    public:
        ~QueryHandle()
        {
            if(m_query->eId == EcsInvalidId)
            {
                Destroy();
            }
        }

        QueryHandle(QueryHandle&& other) noexcept
        {
            m_query = other.m_query;

            other.m_query = nullptr;
        }

        QueryHandle& operator=(QueryHandle&& other) noexcept
        {
            m_query = other.m_query;

            other.m_query = nullptr;

            return *this;
        }

        void Execute();

        // No need to call this manually unless you use cache query
        // Ad-hoc query handle will call this when go out of scope (RAII)
        void Destroy();


    private:
        template<typename Handle>
        friend class QueryCallBackBuilderBase;
        QueryHandle(Query* query) : m_query(query) { assert(m_query); }

    private:
        Query* m_query;
    };

    using SystemHandle = QueryHandle;

    template <typename Handle, typename CallbackBuilder, typename... T>
    class QueryBuilderBase
    {
    protected:
        QueryBuilderBase(World* world)
            : m_world(world), m_currTermIdx(0), m_desc(), m_firstTerm(true)
        {
            static_assert(!std::is_reference_v<Handle> && !std::is_const_v<Handle>);
            static_assert(((!std::is_reference_v<T> && !std::is_const_v<T>) && ...),
                          "Query terms must not be ref or const!");
            assert(m_world);
            (Term<T>(), ...);
        }

    public:
        QueryBuilderBase<Handle, CallbackBuilder, T...>& Term(EntityId id);

        QueryBuilderBase<Handle, CallbackBuilder, T...>& Term(EntityId first, EntityId second);

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& Term();

        QueryBuilderBase<Handle, CallbackBuilder, T...>& Through(TraverseMethod method);

        // QueryBuilder<T...> &TraveseTarget(EntityId targetId);

        QueryBuilderBase<Handle, CallbackBuilder, T...>& Traverse(EntityId relation,
                                                                  EntityId target);

        QueryBuilderBase<Handle, CallbackBuilder, T...>& TraverseAny(EntityId relation);

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& Traverse(EntityId target);

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& TraverseAny();

        QueryBuilderBase<Handle, CallbackBuilder, T...>& Op(TermOp op);

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& With();

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& Without();

        // QueryBuilder<T...> &With();
        //
        // QueryBuilder<T...> &Without();

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& Up(EntityId target);

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& Cascade();

        QueryBuilderBase<Handle, CallbackBuilder, T...>& Up(EntityId relation, EntityId target);

        QueryBuilderBase<Handle, CallbackBuilder, T...>& Cascade(EntityId relation);

        template <typename U>
        QueryBuilderBase<Handle, CallbackBuilder, T...>& Modify();

        template <typename... CallbackArgs>
        CallbackBuilder Filter(bool (*)(CallbackArgs...), void* ctx = nullptr);

        template <typename... CallbackArgs>
        Handle Each(void (*)(CallbackArgs...), void* ctx = nullptr);

    protected:
        Query* BuildQuery();

    protected:
        World* m_world;
        QueryDesc m_desc;
        QueryTerm m_currTerm;
        uint32_t m_currTermIdx;
        bool m_firstTerm;
    };

    template<typename Handle>
    class QueryCallBackBuilderBase
    {
    public:
        template <typename... CallbackArgs>
        QueryCallBackBuilderBase<Handle>& Filter(bool (*)(CallbackArgs...), void* ctx = nullptr);

        template <typename... CallbackArgs>
        Handle Each(void (*)(CallbackArgs...), void* ctx = nullptr);

    protected:
        QueryCallBackBuilderBase(Query* query) : m_query(query) { assert(m_query); }

    protected:
        Query* m_query;
    };

    class QueryCallbackBuilder : public QueryCallBackBuilderBase<QueryHandle>
    {
    public:
        QueryCallbackBuilder& Cache(EntityId eId)
        {
            m_query->isCached = true;
            return *this;
        }

    private:
        template <typename... T>
        friend class QueryBuilder;

        template <typename Handle, typename CallbackBuilder, typename... T>
        friend class QueryBuilderBase;

        QueryCallbackBuilder(Query* query) : QueryCallBackBuilderBase<QueryHandle>(query) {}
    };

    class SystemCallbackBuilder : public QueryCallBackBuilderBase<SystemHandle>
    {
    private:
        template <typename Handle, typename CallbackBuilder, typename... T>
        friend class QueryBuilderBase;

        SystemCallbackBuilder(Query* query) : QueryCallBackBuilderBase<SystemHandle>(query) {}
    };


    template <typename... T>
    class QueryBuilder : public QueryBuilderBase<QueryHandle, QueryCallbackBuilder, T...>
    {
    public:
        QueryCallbackBuilder Cache(EntityId eId);

        QueryBuilder(World* world) : ECS::QueryBuilderBase<QueryHandle, QueryCallbackBuilder, T...>(world)
        {
        }
    };

    template <typename... T>
    class SystemBuilder : public QueryBuilderBase<SystemHandle, SystemCallbackBuilder, T...>
    {
    public:
        SystemBuilder(World* world)
            : ECS::QueryBuilderBase<SystemHandle, SystemCallbackBuilder, T...>(world)
        {
        }
    };
} // namespace ECS
