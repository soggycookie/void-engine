#pragma once
#include "ecs_pch.h"
#include "ecs_type.h"

namespace ECS
{
class Id
{
protected:
    Id(EntityId id) : m_id(id) {}

    Id(LoEntityId lowId, HiEntityId highId)
        : m_id(MAKE_ENTITY_ID(lowId, highId))
    {
    }

    virtual ~Id() = default;

    Id(Id &&other) = default;
    Id(const Id &other) = default;

    Id &operator=(const Id &other) = default;
    Id &operator=(Id &&other) = default;

public:
    EntityId GetFullId() const { return m_id; }

    LoEntityId GetLowId() const { return LO_ENTITY_ID(m_id); }

    HiEntityId GetHighId() const { return HI_ENTITY_ID(m_id); }

    GenCount GetGenCount() const { return ENTITY_GEN_COUNT(m_id); }

    void IncreGenCount()
    {
        if (isValid())
        {
            m_id = INCRE_GEN_COUNT(m_id);
        }
    }

    bool isValid() const { return LO_ENTITY_ID(m_id) != 0; }

protected:
    EntityId m_id;
};

} // namespace ECS
