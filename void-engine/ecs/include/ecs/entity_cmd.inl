#ifdef __clang__
#pragma once
#include "entity_cmd.h"
#endif

namespace ECS
{
    /*
        Definition for entity command base interface's public funcs
    */

    template<typename Component>
    IEntityCommand& IEntityCommand::AddComponent()
    {
        AddComponentImpl(ComponentTypeId<Component>::Id());

        return *this;
    }

    template<typename Component>
    IEntityCommand& IEntityCommand::AddTag()
    {
        AddTagImpl(ComponentTypeId<Component>::Id());

        return *this;    
    }

    template<typename First>
    IEntityCommand& IEntityCommand::AddRelationship(EntityId second)
    {
        AddPairImpl(ComponentTypeId<First>::Id(), second);

        return *this;       
    }

    template<typename Component>
    IEntityCommand& IEntityCommand::RemoveComponent()
    {
        RemoveComponentImpl(ComponentTypeId<Component>::Id());

        return *this;   
    }

    template<typename Component>
    IEntityCommand& IEntityCommand::Set(Component&& c)
    {
        SetImpl(ComponentTypeId<Component>::Id(), &c);

        return *this;
    }

    template<typename Component>
    Component& IEntityCommand::Get()
    {
        return *PTR_CAST(GetImpl(ComponentTypeId<Component>::Id()), Component);
    }
}
