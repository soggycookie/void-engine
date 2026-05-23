#pragma once
#include "ds/hash_map.h"
#include "ds/world_allocator.h"
#include "ecs_pch.h"
#include "internal_component_id.h"

namespace ECS
{

template <typename T>
constexpr const std::string_view GetComponentName()
{
#if defined(__clang__) || defined(__GNUC__)
    std::string_view funcSig = __PRETTY_FUNCTION__;

    size_t start = funcSig.find("T =") + 4;
    size_t end = funcSig.find("]", start);

    std::string_view typeName = funcSig.substr(start, end - start);
    size_t ns = typeName.find_last_of("::", end) + 1;

    if (ns != std::string_view::npos)
    {
        typeName = typeName.substr(ns, end - ns);
    }

    return typeName;

#elif defined(_MSC_VER)
    // const class std::basic_string_view<char,struct std::char_traits<char> >
    // __cdecl ECS::GetComponentName<struct VoidEngine::NPC>(void)
    std::string_view funcSig = __FUNCSIG__;

    size_t start = funcSig.find("<struct");

    if (start == std::string_view::npos)
    {
        start = funcSig.find("<class") + 7;
    }
    else
    {
        start += 8;
    }

    size_t end = funcSig.find_last_of(">(") - 1;
    std::string_view typeName = funcSig.substr(start, end - start);
    size_t ns = typeName.find_last_of("::", end) + 1;

    if (ns != std::string_view::npos)
    {
        typeName = typeName.substr(ns, end - ns);
    }

    return typeName;

#endif

    return std::string_view(nullptr, 0);
}

#define ENTITY_ID_MASK  0xFFFFFFFFULL
#define ENTITY_GEN_MASK 0xFFFFULL

#define LO_ENTITY_ID(x) ((uint32_t)((x) & ENTITY_ID_MASK))

#define HI_ENTITY_ID(x) ((uint32_t)(((x) >> 32) & ENTITY_ID_MASK))

#define ENTITY_GEN_COUNT(x) ((uint16_t)(((x) >> 32) & ENTITY_GEN_MASK))

#define MAKE_ENTITY_ID(lo, hi)                                                 \
    ((((uint64_t)(hi) & ENTITY_ID_MASK) << 32) |                               \
     ((uint64_t)(lo) & ENTITY_ID_MASK))

#define INCRE_GEN_COUNT(x)                                                     \
    MAKE_ENTITY_ID(LO_ENTITY_ID(x), (uint16_t)(ENTITY_GEN_COUNT(x) + 1))

using LoEntityId = uint32_t;
using HiEntityId = uint32_t;
using GenCount = uint16_t;

inline EntityId MakeRelationship(EntityId first, EntityId sec)
{
    constexpr uint32_t mask = 0xFFFFFFFFULL;

    LoEntityId f = first & mask;
    LoEntityId s = sec & mask;

    EntityId relationship = (EntityId)f | ((EntityId)s << 32);

    return relationship;
}

class World;

template <typename T>
class TypeInfoBuilder;

struct TypeInfo;

template <typename T>
class ComponentTypeId
{
private:
    template <typename U>
    friend class TypeInfoBuilder;
    static EntityId id;

    static void Id(EntityId eId) { id = eId; }

public:
    static EntityId Id() { return id; }
};

template <typename T>
EntityId ComponentTypeId<T>::id = 0;

template <typename T>
struct Store
{
    T *data;
    uint32_t count;
    uint32_t capacity;

    Store() : data(nullptr), count(0), capacity(0)
    {
        static_assert(std::is_destructible_v<T>);
    }

    Store(Store &&other) noexcept
    {
        data = other.data;
        count = other.count;
        capacity = other.capacity;

        other.data = nullptr;
    }

    Store &operator=(Store &&other) noexcept
    {
        data = other.data;
        count = other.count;
        capacity = other.capacity;

        other.data = nullptr;

        return *this;
    }

    void Init(WorldAllocator &wAllocator, uint32_t capacity = 8)
    {
        uint32_t storeCapacity = capacity;
        count = 0;
        data = PTR_CAST(
            wAllocator.AllocN(sizeof(T), storeCapacity, storeCapacity), T);
        this->capacity = storeCapacity;
    }

    void Grow(WorldAllocator &wAllocator)
    {
        if (capacity == 0)
        {
            Init(wAllocator, 4);
            return;
        }

        uint32_t newStoreCapacity = capacity * 2;
        T *newStore = PTR_CAST(
            wAllocator.AllocN(sizeof(T), newStoreCapacity, newStoreCapacity),
            T);

        if constexpr (std::is_move_constructible_v<T>)
        {
            for (size_t idx = 0; idx < count; idx++)
            {
                new (&newStore[idx]) T(std::move(data[idx]));
                data[idx].~T();
            }
        }
        else if (std::is_copy_constructible_v<T>)
        {
            for (size_t idx = 0; idx < count; idx++)
            {
                new (&newStore[idx]) T(data[idx]);
                data[idx].~T();
            }
        }
        else
        {
            std::memcpy(newStore, data, sizeof(T) * count);
        }

        wAllocator.Free(sizeof(T) * capacity, data);

        data = newStore;
        capacity = newStoreCapacity;
    }

    template <typename U = T>
    void Add(WorldAllocator &wAllocator, U &&element)
    {
        static_assert(std::is_same_v<std::decay_t<T>, std::decay_t<U>>,
                      "Wrong argument type!");

        if (count == capacity)
        {
            Grow(wAllocator);
        }

        new (&data[count]) T(std::move(element));
        ++count;
    }

    void Clear()
    {
        for (size_t idx = 0; idx < count; ++idx)
        {
            data[idx].~T();
        }
        count = 0;
    }

    void Destroy(WorldAllocator &wAllocator)
    {
        if (data)
        {
            wAllocator.Free(sizeof(T) * capacity, data);
        }
    }

    T &operator[](size_t idx)
    {
        if (idx < count)
        {
            return data[idx];
        }
        assert(0 && "Index out of bound!");
    }

    const T &operator[](size_t idx) const
    {
        if (idx < count)
        {
            return data[idx];
        }
        assert(0 && "Index out of bound!");
    }
};

struct ComponentSet
{
    EntityId *idArr;
    uint32_t count;

    constexpr static const int32_t NotFoundIdx = -1;

    ComponentSet() : idArr(nullptr), count(0) {}

    ~ComponentSet() = default;

    ComponentSet(ComponentSet &&other) noexcept
    {
        idArr = other.idArr;
        count = other.count;

        other.idArr = nullptr;
        other.count = 0;
    }

    ComponentSet &operator=(ComponentSet &&other) noexcept
    {
        idArr = other.idArr;
        count = other.count;

        other.idArr = nullptr;
        other.count = 0;

        return *this;
    }

    bool operator==(const ComponentSet &other)
    {
        if (count != other.count)
        {
            return false;
        }

        for (uint32_t i = 0; i < count; i++)
        {
            if (idArr[i] != other.idArr[i])
            {
                return false;
            }
        }

        return true;
    }

    bool operator!=(const ComponentSet &other)
    {
        if (count == other.count)
        {
            return true;
        }

        for (uint32_t i = 0; i < count; i++)
        {
            if (idArr[i] != other.idArr[i])
            {
                return true;
            }
        }

        return false;
    }

    EntityId &operator[](uint32_t index)
    {
        if (index >= count)
        {
            assert(0);
        }

        return idArr[index];
    }

    const EntityId &operator[](uint32_t index) const
    {
        if (index >= count)
        {
            assert(0);
        }

        return idArr[index];
    }

    uint64_t Hash() const
    {
        uint64_t h = 0;
        for (uint32_t i = 0; i < count; i++)
        {
            h += ECS::HashU64(idArr[i]);
        }

        h /= count;

        return h;
    }

    void Sort()
    {
        if (idArr)
        {
            std::sort(idArr, (idArr + count));
        }
    }

    int32_t Search(EntityId id) const
    {
        EntityId *v = std::lower_bound(idArr, (idArr + count), id);

        if (v == (idArr + count) || *v != id)
        {
            return NotFoundIdx;
        }

        return static_cast<int32_t>(v - idArr);
    }

    int32_t SearchRelationship(EntityId id) const
    {
        uint32_t hiId = HI_ENTITY_ID(id);
        uint32_t loId = LO_ENTITY_ID(id);

        for (uint32_t idx = count; idx > 0;)
        {
            --idx;
            if (hiId == 100 || hiId == 0)
            {
                if (loId == LO_ENTITY_ID(idArr[idx]))
                {
                    return idx;
                }
            }
            else
            {
                if (id == idArr[idx])
                {
                    return idx;
                }
            }

            if (HI_ENTITY_ID(idArr[idx]) == 0)
            {
                return NotFoundIdx;
            }
        }

        return NotFoundIdx;
    }

    bool Has(EntityId id) const
    {
        EntityId *v = std::lower_bound(idArr, (idArr + count), id);

        if (v == (idArr + count) || *v != id)
        {
            return false;
        }

        return true;
    }

    bool HasRelationship(EntityId id) const
    {
        uint32_t hiId = HI_ENTITY_ID(id);
        uint32_t loId = LO_ENTITY_ID(id);

        for (uint32_t idx = count; idx > 0;)
        {
            --idx;
            if (hiId == 100 || hiId == 0)
            {
                if (loId == LO_ENTITY_ID(idArr[idx]))
                {
                    return true;
                }
            }
            else
            {
                if (id == idArr[idx])
                {
                    return true;
                }
            }

            // normal component type
            if (HI_ENTITY_ID(idArr[idx]) == 0)
            {
                return false;
            }
        }

        return false;
    }

    void Init(WorldAllocator &wAllocator, uint32_t count)
    {
        idArr = PTR_CAST(wAllocator.Alloc(count * sizeof(EntityId)), EntityId);
        this->count = count;
    }

    void Destroy(WorldAllocator &wAllocator)
    {
        if (idArr)
        {
            wAllocator.Free(sizeof(EntityId) * count, idArr);
        }
    }

    void Clone(WorldAllocator &wAllocator, const ComponentSet &other)
    {
        count = other.count;
        Init(wAllocator, count);
        std::memcpy(idArr, other.idArr, count * sizeof(EntityId));
    }
};

template <>
struct Hash<ComponentSet>
{
    static uint64_t Value(const ComponentSet &v) { return v.Hash(); }
};

struct Column
{
    void *data;
    TypeInfo *typeInfo;
};

using ComponentDiff = ComponentSet;
using ArchetypeId = uint32_t;

constexpr uint32_t DefaultArchetypeCapacity = 4;

struct TrackedQuery
{
    EntityId id;
    uint32_t idx;
};

struct Archetype
{
    ArchetypeId id;
    uint32_t count;
    uint32_t capacity;
    uint32_t flags;
    Column *columns;
    EntityId *entities;
    ComponentSet componentSet;
    int32_t *columnMap;
    HashMap<EntityId, Archetype *> addEdges;
    HashMap<EntityId, Archetype *> removeEdges;
    Store<TrackedQuery> trackedQueries;
    uint32_t columnCount;

    Archetype()
        : id(0), count(0), capacity(0), flags(0), columns(nullptr),
          entities(nullptr), componentSet(), addEdges(), removeEdges(),
          trackedQueries(), columnCount(0), columnMap()
    {
    }

    Archetype(Archetype &&other) noexcept
    {
        id = other.id;
        count = other.count;
        capacity = other.capacity;
        flags = other.flags;
        columnCount = other.columnCount;
        columns = other.columns;
        entities = other.entities;
        columnMap = other.columnMap;
        componentSet = std::move(other.componentSet);
        addEdges = std::move(other.addEdges);
        removeEdges = std::move(other.removeEdges);
        trackedQueries = std::move(other.trackedQueries);

        other.columns = nullptr;
        other.entities = nullptr;
    }

    Archetype &operator=(Archetype &&other) noexcept
    {
        id = other.id;
        count = other.count;
        capacity = other.capacity;
        flags = other.flags;
        columnCount = other.columnCount;
        columns = other.columns;
        entities = other.entities;
        columnMap = other.columnMap;
        componentSet = std::move(other.componentSet);
        addEdges = std::move(other.addEdges);
        removeEdges = std::move(other.removeEdges);
        trackedQueries = std::move(other.trackedQueries);

        other.columns = nullptr;
        other.entities = nullptr;

        return *this;
    }
};

inline ArchetypeId GetArchetypeId()
{
    static ArchetypeId id = 0;

    return ++id;
}

struct ComponentRecord
{
    EntityId id;
    Store<Archetype *> archetypes;
    TypeInfo *typeInfo;
    Store<EntityId> trackedQueries;
#ifdef ECS_DEBUG
    char name[32];
#endif
};

struct EntityRecord
{
    Archetype *archetype;
    uint32_t row;
    uint32_t dense;
};

// Intrusive ref

class IntrusiveRefCount
{
public:
    virtual void Release() = 0;

    virtual void AddRef() { ++m_refCount; }

protected:
    uint32_t m_refCount = 0;
};

} // namespace ECS
