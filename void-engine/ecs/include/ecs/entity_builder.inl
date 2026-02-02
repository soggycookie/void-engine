
namespace ECS
{
    template<typename Component>
    EntityBuilder& EntityBuilder::AddComponent()
    {
        m_world->AddComponent<Component>(m_id);
        return *this;
    }

    template<typename Component>
    EntityBuilder& EntityBuilder::RemoveComponent()
    {
        return *this;
    }

    template<typename FirstComponent, typename... Components>
    EntityBuilder& EntityBuilder::AddComponents(const FirstComponent& f, const Components&... c)
    {

        return *this;
    }

    template<typename FirstComponent, typename... Components>
    EntityBuilder& EntityBuilder::RemoveComponents()
    {

        return *this;
    }

    template<typename Component>
    EntityBuilder& EntityBuilder::Set(Component&& c)
    {
        m_world->Set<Component>(m_id, std::forward<Component>(c));

        return *this;
    }

    template<typename Component>
    Component& EntityBuilder::Get()
    {
        return m_world->Get<Component>(m_id);
    }
}