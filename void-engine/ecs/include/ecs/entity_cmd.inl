
namespace ECS
{
    /*
        Definition for entity command base interface's public funcs
    */

    template<typename Component>
    IEntityCommand& IEntityCommand::AddComponent()
    {
        AddComponentImpl(ComponentTypeId<Component>::id);

        return *this;
    }

    template<typename Component>
    IEntityCommand& IEntityCommand::AddTag()
    {
        AddTagImpl(ComponentTypeId<Component>::id);

        return *this;    
    }

    template<typename First>
    IEntityCommand& IEntityCommand::AddPair(EntityId second)
    {
        AddPairImpl(ComponentTypeId<First>::id, second);

        return *this;       
    }

    template<typename Component>
    IEntityCommand& IEntityCommand::RemoveComponent()
    {
        RemoveComponentImpl(ComponentTypeId<Component>::id);

        return *this;   
    }

    template<typename Component>
    IEntityCommand& IEntityCommand::Set(Component&& c)
    {
        SetImpl(ComponentTypeId<Component>::id, &c);

        return *this;
    }

    template<typename Component>
    Component& IEntityCommand::Get()
    {
        return *PTR_CAST(GetImpl(ComponentTypeId<Component>::id), Component);
    }
}