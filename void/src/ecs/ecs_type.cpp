#include "ecs/ecs_type.h"

namespace VoidEngine::ECS
{

void Archetype::resize(Vallocator &vallocator)
{
    uint32_t new_capacity = capacity * 2;
    EntityId *new_entities = SCAST(EntityId *, vallocator.valloc(sizeof(EntityId) * new_capacity, alignof(EntityId)));
    ENGINE_ASSERT(new_entities, "Failed to resize entities array of archetype!");
    std::memcpy(new_entities, entities, sizeof(EntityId) * capacity);
    vallocator.vfree(capacity * sizeof(EntityId), entities);
    entities = new_entities;

    // grow column data
    for (uint32_t idx = 0; idx < column_count; idx++)
    {
        Column &col = columns[idx];
        const TypeMetadata *tm = col.type_meta;
        void *new_col_data = vallocator.valloc(tm->size * new_capacity, tm->alignment);

        if (tm->hook.move_ctor || tm->hook.copy_ctor)
        {
            for (uint32_t row = 0; row < count; row++)
            {
                void *dst = OFFSET_ADDR(new_col_data, tm->size * row);
                void *src = OFFSET_ADDR(col.data, tm->size * row);

                if (tm->hook.move_ctor)
                {
                    tm->hook.move_ctor(dst, src);
                }
                else
                {
                    tm->hook.copy_ctor(dst, src);
                }

                if (tm->hook.dtor)
                {
                    tm->hook.dtor(src);
                }
            }
        }
        else
        {
            std::memcpy(new_col_data, col.data, tm->size * count);
        }
        vallocator.vfree(tm->size * capacity, col.data);
        col.data = new_col_data;
    }
    capacity = new_capacity;
}

void Archetype::destroy(Vallocator &vallocator)
{
    for (size_t idx = 0; idx < column_count; idx++)
    {
        Column &col = columns[idx];
        const TypeMetadata &tm = *col.type_meta;
        if (tm.hook.dtor)
        {
            for (uint32_t row = 0; row < count; row++)
            {
                void *component = OFFSET_ADDR(col.data, (tm.size * row));
                tm.hook.dtor(component);
            }
        }
        vallocator.vfree(tm.size * capacity, col.data);
    }

    vallocator.vfree(capacity * sizeof(EntityId), entities);
    vallocator.vfree(type_set.count * 2 * sizeof(int32_t), column_map);
    vallocator.vfree(type_set.count * sizeof(Column), columns);
    type_set.destroy(vallocator);
    tracked_queries.destroy(vallocator);
    add_edges.destroy(vallocator);
    del_edges.destroy(vallocator);
}

void Archetype::init(Vallocator &vallocator, ArchetypeId id, TypeSet &&tset,
                     const FlatHashMap<EntityId, TypeInfo> &type_infos)
{
    const uint32_t edge_capa = 8;
    const uint32_t default_capa = 8;
    this->id = id;
    count = 0;
    capacity = default_capa;
    type_set.move(vallocator, std::move(tset));
    tracked_queries.init(vallocator, 4);
    add_edges.init(vallocator, edge_capa);
    del_edges.init(vallocator, edge_capa);

#ifdef VOID_DEBUG
    uint32_t cur = 0;
    for(size_t idx = 0; idx < type_set.count; ++idx)
    {
        const TypeInfo &cr = type_infos[type_set[idx]];
        uint32_t len = std::strlen(cr.name);
        if (cur + len + 1 >= sizeof(name)) {
            break;
        }
        std::memcpy(name + cur, cr.name, len);
        name[cur + len] = ' ';
        cur += (len +1);
    }
    name[cur] = '\0';

#endif // VOID_DEBUG

    columns = SCAST(Column *, vallocator.valloc(sizeof(Column) * type_set.count, alignof(Column)));
    column_map = SCAST(int32_t *, vallocator.valloc(sizeof(int32_t) * type_set.count * 2, alignof(int32_t)));
    entities = SCAST(EntityId *, vallocator.valloc(capacity * sizeof(EntityId), alignof(EntityId)));

    // Set up component map to determine what column has data
    uint32_t data_col_counter = 0;
    for (uint32_t idx = 0; idx < type_set.count; ++idx)
    {
        const TypeMetadata *tm = type_infos[low_32bit(type_set[idx])].meta;
        if (tm->has_data())
        {
            columns[data_col_counter].type_meta = tm;
            columns[data_col_counter].data = vallocator.valloc(tm->size * default_capa, tm->alignment);
            column_map[idx] = data_col_counter;
            column_map[tset.count + data_col_counter] = idx;
            ++data_col_counter;
        }
        else
        {
            column_map[idx] = Archetype::INVALID_COLUMN;
        }
    }
    column_count = data_col_counter;
}

void TypeInfo::init(Vallocator &vallocator, const TypeMetadata *type_meta)
{
    archetypes.init(vallocator, 8);
    tracked_queries.init(vallocator, 8);
    meta = type_meta;
}

void TypeInfo::destroy(Vallocator &vallocator)
{
    archetypes.destroy(vallocator);
    tracked_queries.destroy(vallocator);
}

void TypeSet::move(Vallocator &vallocator, TypeSet &&other)
{
    destroy(vallocator);
    ids = other.ids;
    count = other.count;

    other.ids = nullptr;
    other.count = 0;
}

uint64_t TypeSet::hash() const
{
    uint64_t h = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        h += VoidEngine::Hash<uint64_t>::Value(ids[i]);
    }

    h /= count;

    return h;
}

void TypeSet::sort()
{
    if (ids)
    {
        std::sort(ids, (ids + count));
    }
}

int32_t TypeSet::search(EntityId id) const
{
    uint32_t hi_id = high_32bit(id);
    uint32_t lo_id = low_32bit(id);
    bool wildcard = hi_id == WILDCARD_ID;
    if (!wildcard)
    {
        EntityId *v = std::lower_bound(ids, (ids + count), id);
        if (v == (ids + count) || *v != id)
        {
            return NOT_FOUND_IDX;
        }

        return SCAST(int32_t, v - ids);
    }
    else
    {
        for (uint32_t idx = count; idx > 0;)
        {
            --idx;
            if (lo_id == low_32bit(ids[idx]))
            {
                return idx;
            }
            // early exit because relationships always have high id
            if (high_32bit(ids[idx]) == ENTITY_INVALID_ID)
            {
                return NOT_FOUND_IDX;
            }
        }

        return NOT_FOUND_IDX;
    }
}

bool TypeSet::has(EntityId id) const { return search(id) != NOT_FOUND_IDX; }

void TypeSet::init_add(Vallocator &vallocator, const TypeSet *other, EntityId add_tid)
{
    ENGINE_ASSERT(add_tid != ENTITY_INVALID_ID, "Additional type id must not be 0!");
    uint32_t base_count = other ? other->count : 0;
    uint32_t new_count = base_count + 1;
    ids = SCAST(EntityId *, vallocator.valloc(new_count * sizeof(EntityId), alignof(EntityId)));
    ENGINE_ASSERT(ids, "Failed to alloc typeset!");
    if (other)
    {
        std::memcpy(ids, other->ids, base_count * sizeof(EntityId));
    }
    ids[base_count] = add_tid;
    sort();
    this->count = new_count;
}

void TypeSet::init_remove(Vallocator &vallocator, const TypeSet *other, EntityId remove_tid)
{
    ENGINE_ASSERT(remove_tid != ENTITY_INVALID_ID, "Additional type id must not be 0!");
    ENGINE_ASSERT(other, "Other typeset must not be null!");
    int32_t ridx = other->search(remove_tid);
    ENGINE_ASSERT(ridx != TypeSet::NOT_FOUND_IDX, "Component does not exist in archetype to remove!");
    uint32_t new_count = other->count - 1;
    ids = SCAST(EntityId *, vallocator.valloc(new_count * sizeof(EntityId), alignof(EntityId)));
    ENGINE_ASSERT(ids, "Failed to alloc typeset!");

    std::memcpy(ids, other->ids, ridx * sizeof(EntityId));
    std::memcpy(ids + ridx, other->ids + ridx + 1, (count - ridx - 1) * sizeof(EntityId));
    sort();
    this->count = other->count - 1;
}

void TypeSet::destroy(Vallocator &vallocator)
{
    vallocator.vfree(sizeof(EntityId) * count, ids);
    ids = nullptr;
    count = 0;
}

// void TypeSet::clone(Vallocator &vallocator, const TypeSet &other)
// {
//     std::memcpy(ids, other.ids, count * sizeof(EntityId));
// }
} // namespace VoidEngine::ECS
