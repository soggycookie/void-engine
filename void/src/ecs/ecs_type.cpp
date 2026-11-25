#include "ecs/ecs_type.h"

namespace VoidEngine::ECS
{

void Archetype::resize(Vallocator &vallocator)
{
    uint32_t new_capacity = capacity_ * 2;
    EntityId *new_entities =
        SCAST(EntityId *, vallocator.valloc(sizeof(EntityId) * new_capacity, alignof(EntityId)));
    ENGINE_ASSERT(new_entities, "Failed to resize entities array of archetype!");
    std::memcpy(new_entities, entities_, sizeof(EntityId) * capacity_);
    vallocator.vfree(capacity_ * sizeof(EntityId), entities_);
    entities_ = new_entities;

    // grow column data
    for (uint32_t idx = 0; idx < column_count_; idx++)
    {
        Column &col = columns_[idx];
        const TypeMetadata *tm = col.type_meta;
        void *new_col_data = vallocator.valloc(tm->size * new_capacity, tm->alignment);

        if (tm->hook.move_ctor || tm->hook.copy_ctor)
        {
            for (uint32_t row = 0; row < count_; row++)
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
            std::memcpy(new_col_data, col.data, tm->size * count_);
        }
        vallocator.vfree(tm->size * capacity_, col.data);
        col.data = new_col_data;
    }
    capacity_ = new_capacity;
}

void Archetype::destroy(Vallocator &vallocator)
{
    for (size_t idx = 0; idx < column_count_; idx++)
    {
        Column &col = columns_[idx];
        const TypeMetadata &tm = *col.type_meta;
        if (tm.hook.dtor)
        {
            for (uint32_t row = 0; row < count_; row++)
            {
                void *component = OFFSET_ADDR(col.data, (tm.size * row));
                tm.hook.dtor(component);
            }
        }
        vallocator.vfree(tm.size * capacity_, col.data);
    }

    vallocator.vfree(capacity_ * sizeof(EntityId), entities_);
    vallocator.vfree(type_set_.count_ * 2 * sizeof(int32_t), column_map_);
    vallocator.vfree(type_set_.count_ * sizeof(Column), columns_);
    type_set_.destroy(vallocator);
    tracked_queries_.destroy(vallocator);
    add_edges_.destroy(vallocator);
    del_edges_.destroy(vallocator);
}

void Archetype::init(Vallocator &vallocator, ArchetypeId id, TypeSet &&tset,
                     const FlatHashMap<EntityId, TypeInfo> &type_infos)
{
    const uint32_t edge_capa = 8;
    const uint32_t default_capa = 8;
    this->id_ = id;
    count_ = 0;
    capacity_ = default_capa;
    type_set_.move(vallocator, std::move(tset));
    tracked_queries_.init(vallocator, 4);
    add_edges_.init(vallocator, edge_capa);
    del_edges_.init(vallocator, edge_capa);

#ifdef VOID_DEBUG
    uint32_t cur = 0;
    for (size_t idx = 0; idx < type_set_.count_; ++idx)
    {
        const TypeInfo &cr = type_infos[type_set_[idx]];
        uint32_t len = std::strlen(cr.name_);
        if (cur + len + 1 >= sizeof(name_))
        {
            break;
        }
        std::memcpy(name_ + cur, cr.name_, len);
        name_[cur + len] = ' ';
        cur += (len + 1);
    }
    name_[cur] = '\0';

#endif // VOID_DEBUG

    columns_ = SCAST(Column *, vallocator.valloc(sizeof(Column) * type_set_.count_, alignof(Column)));
    column_map_ =
        SCAST(int32_t *, vallocator.valloc(sizeof(int32_t) * type_set_.count_ * 2, alignof(int32_t)));
    entities_ = SCAST(EntityId *, vallocator.valloc(capacity_ * sizeof(EntityId), alignof(EntityId)));

    // Set up component map to determine what column has data
    uint32_t data_col_counter = 0;
    for (uint32_t idx = 0; idx < type_set_.count_; ++idx)
    {
        const TypeMetadata *tm = type_infos[low_32bit(type_set_[idx])].meta_;
        if (tm->has_data())
        {
            columns_[data_col_counter].type_meta = tm;
            columns_[data_col_counter].data = vallocator.valloc(tm->size * default_capa, tm->alignment);
            column_map_[idx] = data_col_counter;
            column_map_[type_set_.count_ + data_col_counter] = idx;
            ++data_col_counter;
        }
        else
        {
            column_map_[idx] = Archetype::INVALID_COLUMN;
        }
    }
    column_count_ = data_col_counter;
}

void TypeInfo::init(Vallocator &vallocator, const TypeMetadata *type_meta)
{
    archetypes_.init(vallocator, 8);
    tracked_queries_.init(vallocator, 8);
    meta_ = type_meta;
}

void TypeInfo::destroy(Vallocator &vallocator)
{
    archetypes_.destroy(vallocator);
    tracked_queries_.destroy(vallocator);
}

void TypeSet::move(Vallocator &vallocator, TypeSet &&other)
{
    destroy(vallocator);
    ids_ = other.ids_;
    count_ = other.count_;

    other.ids_ = nullptr;
    other.count_ = 0;
}

uint64_t TypeSet::hash() const
{
    if (count_ == 0)
    {
        return 0;
    }
    uint64_t h = 0;
    for (uint32_t i = 0; i < count_; i++)
    {
        h += VoidEngine::Hash<uint64_t>::Value(ids_[i]);
    }

    h /= count_;

    return h;
}

void TypeSet::sort()
{
    if (ids_)
    {
        std::sort(ids_, (ids_ + count_));
    }
}

int32_t TypeSet::search(EntityId id) const
{
    uint32_t hi_id = high_32bit(id);
    uint32_t lo_id = low_32bit(id);
    bool wildcard = hi_id == WILDCARD_ID;
    if (!wildcard)
    {
        EntityId *v = std::lower_bound(ids_, (ids_ + count_), id);
        if (v == (ids_ + count_) || *v != id)
        {
            return NOT_FOUND_IDX;
        }

        return SCAST(int32_t, v - ids_);
    }
    else
    {
        for (uint32_t idx = count_; idx > 0;)
        {
            --idx;
            if (lo_id == low_32bit(ids_[idx]))
            {
                return idx;
            }
            // early exit because relationships always have high id
            if (high_32bit(ids_[idx]) == ENTITY_INVALID_ID)
            {
                return NOT_FOUND_IDX;
            }
        }
        return NOT_FOUND_IDX;
    }
}

bool TypeSet::has(EntityId id) const
{
    return search(id) != NOT_FOUND_IDX;
}

void TypeSet::init_add(Vallocator &vallocator, const TypeSet *other, EntityId add_tid)
{
    ENGINE_ASSERT(add_tid != ENTITY_INVALID_ID, "Additional type id must not be 0!");
    uint32_t base_count = other ? other->count_ : 0;
    uint32_t new_count = base_count + 1;
    ids_ = SCAST(EntityId *, vallocator.valloc(new_count * sizeof(EntityId), alignof(EntityId)));
    ENGINE_ASSERT(ids_, "Failed to alloc typeset!");
    if (other)
    {
        std::memcpy(ids_, other->ids_, base_count * sizeof(EntityId));
    }
    ids_[base_count] = add_tid;
    this->count_ = new_count;
    sort();
}

void TypeSet::init_remove(Vallocator &vallocator, const TypeSet *other, EntityId remove_tid)
{
    ENGINE_ASSERT(remove_tid != ENTITY_INVALID_ID, "Additional type id must not be 0!");
    ENGINE_ASSERT(other, "Other typeset must not be null!");
    int32_t ridx = other->search(remove_tid);
    ENGINE_ASSERT(ridx != TypeSet::NOT_FOUND_IDX, "Component does not exist in archetype to remove!");
    uint32_t new_count = other->count_ - 1;
    if (new_count == 0)
    {
        return;
    }
    ids_ = SCAST(EntityId *, vallocator.valloc(new_count * sizeof(EntityId), alignof(EntityId)));
    ENGINE_ASSERT(ids_, "Failed to alloc typeset!");

    std::memcpy(ids_, other->ids_, ridx * sizeof(EntityId));
    std::memcpy(ids_ + ridx, other->ids_ + ridx + 1, (other->count_ - ridx - 1) * sizeof(EntityId));
    this->count_ = other->count_ - 1;
}

void TypeSet::destroy(Vallocator &vallocator)
{
    vallocator.vfree(sizeof(EntityId) * count_, ids_);
    ids_ = nullptr;
    count_ = 0;
}

// void TypeSet::clone(Vallocator &vallocator, const TypeSet &other)
// {
//     std::memcpy(ids, other.ids, count * sizeof(EntityId));
// }
} // namespace VoidEngine::ECS
