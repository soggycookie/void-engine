#pragma once
#include "dynamic_array.h"
#include "flat_hash_map.h"
#include "internal_component_id.h"
#include "type_info.h"
#include "utils.h"

namespace VoidEngine::ECS
{
template <typename T>
constexpr const std::string_view get_type_name()
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

inline EntityId make_relationship(EntityId first, EntityId sec)
{
    constexpr uint32_t mask = 0xFFFFFFFFULL;
    LowEntityId f = first & mask;
    LowEntityId s = sec & mask;
    return make_uint64(f, s);
}

class World;

template <typename T>
class TypeMetaBuilder;

struct TypeMetadata;

template <typename T>
class TypeId
{
private:
    friend class World;
    static EntityId s_id;

    static void set_id(EntityId eid)
    {
        s_id = eid;
    }

public:
    static EntityId id()
    {
        return s_id;
    }
};

template <typename T>
EntityId TypeId<T>::s_id = 0;

struct TypeSet
{
    EntityId *ids;
    uint32_t count;

    constexpr static int32_t NOT_FOUND_IDX = -1;

    TypeSet() : ids(nullptr), count(0)
    {
    }

    NO_COPY_ASSIGNMENT(TypeSet);
    NO_MOVE_ASSIGNMENT(TypeSet);

    TypeSet(TypeSet &&other) noexcept : ids(other.ids), count(other.count)
    {
        other.ids = nullptr;
        other.count = 0;
    }

    // shallow copy
    TypeSet(const TypeSet &other) : ids(other.ids), count(other.count)
    {
    }

    bool operator==(const TypeSet &other)
    {
        if (count != other.count)
        {
            return false;
        }

        return std::memcmp(ids, other.ids, count * sizeof(EntityId)) == 0;
    }

    bool operator!=(const TypeSet &other)
    {
        if (count != other.count)
        {
            return true;
        }

        return std::memcmp(ids, other.ids, count * sizeof(EntityId)) != 0;
    }

    EntityId &operator[](uint32_t idx)
    {
        ENGINE_ASSERT(idx < count, "Index is out of bound!");
        return ids[idx];
    }

    const EntityId &operator[](uint32_t idx) const
    {
        ENGINE_ASSERT(idx < count, "Index is out of bound!");
        return ids[idx];
    }

    friend void swap(TypeSet &a, TypeSet &b) noexcept
    {
        using std::swap;

        swap(a.ids, b.ids);
        swap(a.count, b.count);
    }

    void move(Vallocator &vallocator, TypeSet &&other);
    // void clone(Vallocator &vallocator, const TypeSet &other);

    uint64_t hash() const;
    void sort();

    int32_t search(EntityId id) const;
    bool has(EntityId id) const;

    void init_add(Vallocator &vallocator, const TypeSet *other, EntityId add_tid);
    void init_remove(Vallocator &vallocator, const TypeSet *other, EntityId remove_tid);
    void destroy(Vallocator &vallocator);
};

// TODO: add capacity to each column to avoid unnecessary grow when the space is enough
struct Column
{
    void *data;
    const TypeMetadata *type_meta;
};

using ComponentDiff = TypeSet;
using ArchetypeId = uint32_t;
using QueryId = uint32_t;

inline ArchetypeId get_archetype_id()
{
    static ArchetypeId id = 0;
    return ++id;
}

inline QueryId get_query_id()
{
    static QueryId id = 0;
    return ++id;
}

struct TrackedQuery
{
    LowEntityId id;
    uint32_t queryArchetypeIdx;
};

struct Archetype;

struct EntityRecord
{
    EntityRecord() : archetype(nullptr), row(0)
    {
    }

    Archetype *archetype;
    uint32_t row;
    uint32_t padding;
    // uint32_t dense;
};

struct TypeInfo
{
    const TypeMetadata *meta;
    DynamicArray<Archetype *> archetypes;
    DynamicArray<EntityId> tracked_queries;
#ifdef VOID_DEBUG
    char name[32];
#endif

    TypeInfo() : meta(nullptr), archetypes(), tracked_queries()
    {
    }

    TypeInfo(TypeInfo &&other)
        : meta(other.meta), archetypes(std::move(other.archetypes)),
          tracked_queries(std::move(other.tracked_queries))
    {
        std::memcpy(name, other.name, 32);
        other.meta = nullptr;
    }

    void init(Vallocator &vallocator, const TypeMetadata *type_meta);
    void destroy(Vallocator &vallocator);

    friend void swap(TypeInfo &lhs, TypeInfo &rhs) noexcept
    {
        using std::swap;

        swap(lhs.meta, rhs.meta);
        swap(lhs.archetypes, rhs.archetypes);
        swap(lhs.tracked_queries, rhs.tracked_queries);

#ifdef VOID_DEBUG
        char temp[sizeof(lhs.name)];
        std::memcpy(temp, lhs.name, sizeof(temp));
        std::memcpy(lhs.name, rhs.name, sizeof(lhs.name));
        std::memcpy(rhs.name, temp, sizeof(temp));
#endif
    }
};

struct Archetype
{
#ifdef VOID_DEBUG
    char name[128];
#endif // VOID_DEBUG
    uint32_t count;
    uint32_t capacity;
    Column *columns;
    EntityId *entities;
    TypeSet type_set;
    int32_t *column_map; // map from component index to column index
    FlatHashMap<EntityId, Archetype *> add_edges;
    FlatHashMap<EntityId, Archetype *> del_edges;
    DynamicArray<TrackedQuery> tracked_queries;
    uint32_t column_count;
    ArchetypeId id;

    constexpr static int32_t INVALID_COLUMN = -1;

    Archetype()
        : id(0), count(0), capacity(0), columns(nullptr), entities(nullptr), type_set(), add_edges(),
          del_edges(), column_count(0), column_map(), tracked_queries()
    {
    }

    NO_MOVE_ASSIGNMENT(Archetype);
    NO_COPY_ASSIGNMENT(Archetype);
    NO_COPY_CTOR(Archetype);

    Archetype(Archetype &&other) noexcept
        : id(other.id), count(other.count), capacity(other.capacity), column_count(other.column_count),
          columns(other.columns), entities(other.entities), column_map(other.column_map),
          type_set(std::move(other.type_set)), add_edges(std::move(other.add_edges)),
          del_edges(std::move(other.del_edges)), tracked_queries(std::move(other.tracked_queries))
    {
#ifdef VOID_DEBUG
        std::memcpy(name, other.name, 128);
#endif // VOID_DEBUG
        other.columns = nullptr;
        other.entities = nullptr;
        other.column_map = nullptr;
        other.column_count = 0;
        other.count = 0;
    }

    void resize(Vallocator &vallocator);
    void init(Vallocator &vallocator, ArchetypeId id, TypeSet &&type_set,
              const FlatHashMap<EntityId, TypeInfo> &type_infos);
    void destroy(Vallocator &vallocator);
};

// Intrusive ref

class IntrusiveRefCount
{
public:
    virtual void release() = 0;

    virtual void add_ref()
    {
        ++ref_count;
    }

protected:
    uint32_t ref_count = 0;
};

} // namespace VoidEngine::ECS

namespace VoidEngine
{
template <>
struct Hash<ECS::TypeSet>
{
    static uint64_t Value(const ECS::TypeSet &v)
    {
        return v.hash();
    }
};
} // namespace VoidEngine
