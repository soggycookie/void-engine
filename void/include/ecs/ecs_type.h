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

// TODO(relationship-policy): pairs keep only the low 32 bits, so a pair whose target was destroyed
// can silently re-target a new entity that reuses the slot. Decide a policy: cascade-delete children,
// have World::remove_entity strip the pairs, or detect dangling pairs via versioned target ids.
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
    EntityId *ids_;
    uint32_t count_;

    constexpr static int32_t NOT_FOUND_IDX = -1;

    TypeSet() : ids_(nullptr), count_(0)
    {
    }

    NO_COPY_ASSIGNMENT(TypeSet);
    NO_MOVE_ASSIGNMENT(TypeSet);

    TypeSet(TypeSet &&other) noexcept : ids_(other.ids_), count_(other.count_)
    {
        other.ids_ = nullptr;
        other.count_ = 0;
    }

    // shallow copy
    TypeSet(const TypeSet &other) : ids_(other.ids_), count_(other.count_)
    {
    }

    bool operator==(const TypeSet &other)
    {
        if (count_ != other.count_)
        {
            return false;
        }

        if (count_ == 0)
        {
            return true;
        }

        return std::memcmp(ids_, other.ids_, count_ * sizeof(EntityId)) == 0;
    }

    bool operator!=(const TypeSet &other)
    {
        if (count_ != other.count_)
        {
            return true;
        }

        if (count_ == 0)
        {
            return false;
        }

        return std::memcmp(ids_, other.ids_, count_ * sizeof(EntityId)) != 0;
    }

    EntityId &operator[](uint32_t idx)
    {
        ENGINE_ASSERT(idx < count_, "Index is out of bound!");
        return ids_[idx];
    }

    const EntityId &operator[](uint32_t idx) const
    {
        ENGINE_ASSERT(idx < count_, "Index is out of bound!");
        return ids_[idx];
    }

    friend void swap(TypeSet &a, TypeSet &b) noexcept
    {
        using std::swap;

        swap(a.ids_, b.ids_);
        swap(a.count_, b.count_);
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
    const TypeMetadata *meta_;
    DynamicArray<Archetype *> archetypes_;
    DynamicArray<EntityId> tracked_queries_;
#ifdef VOID_DEBUG
    char name_[32];
#endif

    TypeInfo() : meta_(nullptr), archetypes_(), tracked_queries_()
    {
    }

    TypeInfo(TypeInfo &&other)
        : meta_(other.meta_), archetypes_(std::move(other.archetypes_)),
          tracked_queries_(std::move(other.tracked_queries_))
    {
#ifdef VOID_DEBUG
        std::memcpy(name_, other.name_, 32);
#endif // VOID_DEBUG
        other.meta_ = nullptr;
    }

    void init(Vallocator &vallocator, const TypeMetadata *type_meta);
    void destroy(Vallocator &vallocator);

    friend void swap(TypeInfo &lhs, TypeInfo &rhs) noexcept
    {
        using std::swap;

        swap(lhs.meta_, rhs.meta_);
        swap(lhs.archetypes_, rhs.archetypes_);
        swap(lhs.tracked_queries_, rhs.tracked_queries_);

#ifdef VOID_DEBUG
        char temp[sizeof(lhs.name_)];
        std::memcpy(temp, lhs.name_, sizeof(temp));
        std::memcpy(lhs.name_, rhs.name_, sizeof(lhs.name_));
        std::memcpy(rhs.name_, temp, sizeof(temp));
#endif
    }
};

struct Archetype
{
#ifdef VOID_DEBUG
    char name_[128];
#endif // VOID_DEBUG
    uint32_t count_;
    uint32_t capacity_;
    Column *columns_;
    EntityId *entities_;
    TypeSet type_set_;
    int32_t *column_map_; // map from component index to column index
    FlatHashMap<EntityId, Archetype *> add_edges_;
    FlatHashMap<EntityId, Archetype *> del_edges_;
    DynamicArray<TrackedQuery> tracked_queries_;
    uint32_t column_count_;
    ArchetypeId id_;

    constexpr static int32_t INVALID_COLUMN = -1;

    Archetype()
        : id_(0), count_(0), capacity_(0), columns_(nullptr), entities_(nullptr), type_set_(), add_edges_(),
          del_edges_(), column_count_(0), column_map_(), tracked_queries_()
    {
    }

    NO_MOVE_ASSIGNMENT(Archetype);
    NO_COPY_ASSIGNMENT(Archetype);
    NO_COPY_CTOR(Archetype);

    Archetype(Archetype &&other) noexcept
        : id_(other.id_), count_(other.count_), capacity_(other.capacity_), column_count_(other.column_count_),
          columns_(other.columns_), entities_(other.entities_), column_map_(other.column_map_),
          type_set_(std::move(other.type_set_)), add_edges_(std::move(other.add_edges_)),
          del_edges_(std::move(other.del_edges_)), tracked_queries_(std::move(other.tracked_queries_))
    {
#ifdef VOID_DEBUG
        std::memcpy(name_, other.name_, 128);
#endif // VOID_DEBUG
        other.columns_ = nullptr;
        other.entities_ = nullptr;
        other.column_map_ = nullptr;
        other.column_count_ = 0;
        other.count_ = 0;
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
