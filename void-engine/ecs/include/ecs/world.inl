#include "ecs_type.h"
#ifdef __clang__
#pragma once    
#include "world.h"
#endif

namespace ECS
{
    template<typename T>
    TypeInfoBuilder<T> World::Component()
    {
        //TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
        static_assert(std::is_destructible_v<T>);
        static_assert(std::is_trivially_constructible_v<T>);
        static_assert(sizeof(T) != 1);

        TypeInfoBuilder<T> tiBuilder(this);
        tiBuilder.Component();
        
        tiBuilder.Ctor(
            [](void* dest)
            {
                new (dest) T();
            }
        );

        if constexpr(std::is_move_constructible_v<T>)
        {
            tiBuilder.MoveCtor(
                [](void* dest, void* src)
                {
                    new (dest) T(std::move(*PTR_CAST(src, T)));
                }
            );
        }
        else if constexpr(!std::is_trivially_constructible_v<T>)
        {
            tiBuilder.CopyCtor(
                [](void* dest, const void* src)
                {
                    new (dest) T(*PTR_CAST(src, T));
                }
            );
        }

        if constexpr(!std::is_trivially_destructible_v<T>)
        {
            tiBuilder.Dtor(
                [](void* src)
                {
                    T* c = PTR_CAST(src, T);
                    c->~T();
                }
            );
        }

#ifdef ECS_DEBUG
        tiBuilder.AddEvent(
            []()
            {
                std::cout << "Add component " << GetComponentName<T>() << std::endl;
            }
        );

        tiBuilder.RemoveEvent(
            []()
            {
                std::cout << "Remove component " << GetComponentName<T>() << std::endl;
            }
        );

        tiBuilder.SetEvent(
            [](void* dest)
            {
                std::cout << "Set component " << GetComponentName<T>() << std::endl;
            }
        );
#endif //ECS_DEBUG
       
        return tiBuilder;
    }

    template<typename T>
    TypeInfoBuilder<T> World::Tag()
    {
        //TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();

        static_assert(std::is_destructible_v<T>);
        static_assert(std::is_trivially_constructible_v<T>);

        TypeInfoBuilder<T> tiBuilder(this);
        tiBuilder.Tag();

        assert(tiBuilder.ti.size == 1 && "Tag can not have data");

#ifdef ECS_DEBUG
        tiBuilder.AddEvent(
            []()
            {
                std::cout << "Add tag " << GetComponentName<T>() << std::endl;
            }
        );

        tiBuilder.RemoveEvent(
            []()
            {
                std::cout << "Remove tag " << GetComponentName<T>() << std::endl;
            }
        );
#endif // ECS_DEBUG
        return tiBuilder;
    }

    template<typename T>
    TypeInfoBuilder<T> World::Relation()
    {
        static_assert(std::is_destructible_v<T>);
        static_assert(std::is_trivially_constructible_v<T>);

        TypeInfoBuilder<T> tiBuilder(this);
        tiBuilder.Relation();
        
        if(tiBuilder.ti.size > 1)
        {
            tiBuilder.HasData();

            tiBuilder.Ctor(
                [](void* dest)
                {
                    new (dest) T();
                }
            );

            if constexpr(std::is_move_constructible_v<T>)
            {
                tiBuilder.MoveCtor(
                    [](void* dest, void* src)
                    {
                        new (dest) T(std::move(*PTR_CAST(src, T)));
                    }
                );
            }
            else if constexpr(!std::is_trivially_constructible_v<T>)
            {
                tiBuilder.CopyCtor(
                    [](void* dest, const void* src)
                    {
                        new (dest) T(*PTR_CAST(src, T));
                    }
                );
            }

            if constexpr(!std::is_trivially_destructible_v<T>)
            {
                tiBuilder.Dtor(
                    [](void* src)
                    {
                        T* c = PTR_CAST(src, T);
                        c->~T();
                    }
                );
            }
        }
#ifdef ECS_DEBUG
        tiBuilder.AddEvent(
            []()
            {
                std::cout << "Add relation " << GetComponentName<T>() << std::endl;
            }
        );

        tiBuilder.RemoveEvent(
            []()
            {
                std::cout << "Remove relation " << GetComponentName<T>() << std::endl;
            }
        );

        tiBuilder.SetEvent(
            [](void* dest)
            {
                std::cout << "Set relation " << GetComponentName<T>() << std::endl;
            }
        );
#endif // ECS_DEBUG
        return tiBuilder;
    }

    template<typename T>
    TypeInfoBuilder<T> World::Relationship(EntityId targetId)
    {       
        if(targetId == 0 || 
                !m_entityIndex.isValidDense(targetId) || 
                !m_entityIndex.isValidDense(ComponentTypeId<T>::Id()))
        {
            assert(0);
        }

        assert(std::is_destructible_v<T>);
        assert(std::is_trivially_constructible_v<T>);
        
        TypeInfoBuilder<T> tiBuilder(this);

        tiBuilder.ti.size = sizeof(T);
        tiBuilder.ti.alignment = alignof(T);
        tiBuilder.ti.eId = 0;

        tiBuilder.Relationship(targetId);

        if(tiBuilder.ti.size > 1)
        {
            tiBuilder.Ctor(
                [](void* dest)
                {
                    new (dest) T();
                }
            );

            if constexpr(std::is_move_constructible_v<T>)
            {
                tiBuilder.MoveCtor(
                    [](void* dest, void* src)
                    {
                        new (dest) T(std::move(*PTR_CAST(src, T)));
                    }
                );
            }
            else if constexpr(!std::is_trivially_constructible_v<T>)
            {
                tiBuilder.CopyCtor(
                    [](void* dest, const void* src)
                    {
                        new (dest) T(*PTR_CAST(src, T));
                    }
                );
            }

            if constexpr(!std::is_trivially_destructible_v<T>)
            {
                tiBuilder.Dtor(
                    [](void* src)
                    {
                        T* c = PTR_CAST(src, T);
                        c->~T();
                    }
                );
            }
        }

#ifdef ECS_DEBUG
        tiBuilder.AddEvent(
            []()
            {
                std::cout << "Add relationship " << GetComponentName<T>() << std::endl;
            }
        );

        tiBuilder.RemoveEvent(
            []()
            {
                std::cout << "Remove relationship" << GetComponentName<T>() << std::endl;
            }
        );

        tiBuilder.SetEvent(
            [](void* dest)
            {
                std::cout << "Set relationship" << GetComponentName<T>() << std::endl;
            }
        );
#endif //ECS_DEBUG
        return tiBuilder;
    }

    template<typename T>
    void World::AddComponent(EntityId eId)
    {
        AddComponent(eId, ComponentTypeId<T>::Id());
    }

    template<typename T>
    void World::AddRelationship(EntityId eId, EntityId targetId)
    { 
        AddRelationship(eId, ComponentTypeId<T>::Id(), targetId);
    }

    template<typename T>
    void World::AddTag(EntityId eId)
    {
        AddTag(eId, ComponentTypeId<T>::Id());
    }

    template<typename T>
    void World::RemoveComponent(EntityId eId)
    {
        RemoveComponent(eId, ComponentTypeId<T>::Id());
    }

    template<typename T>
    void World::Set(EntityId eId, T&& c)
    {
        Set(eId, ComponentTypeId<decay_t<T>>::Id(), &c);
    }

    template<typename T>
    T& World::Get(EntityId eId)
    {
        void* data = Get(eId, ComponentTypeId<T>::Id());

        T& component = *PTR_CAST(data, T);

        return component;
    }

    template<typename... Components, typename... FuncArgs>
    void World::System(void (*func)(FuncArgs...))
    {
        EntityId ids[] = {ComponentTypeId<Components>::Id()...};
        uint32_t count = sizeof...(Components);
        ComponentSet componentSet;
        componentSet.Init(m_wAllocator, count);
        std::memcpy(componentSet.idArr, ids, count * sizeof(EntityId));

        ArchetypeLinkedList* node = ArchetypeLinkedList::Init(m_wAllocator);

        ArchetypeLinkedList* head = node;

        SystemCallback sc = CreateSystemCallback<Components..., FuncArgs...>(func);

        for(uint32_t idx = 0; idx < 1; idx++)
        {
            ComponentRecord& cr = m_componentIndex.GetValue(ids[idx]);

            for(uint32_t aIdx = 0; aIdx < cr.archetypeStore.count; aIdx++)
            {
                Archetype* archetype = cr.archetypeStore.store[aIdx];
                bool skip = false;
                assert(archetype);

                for(uint32_t remainIdx = 1; remainIdx < count; remainIdx++)
                {
                    //Archetype does not contain the same set of components
                    if(!archetype->componentSet.Has(ids[remainIdx]) &&
                       !archetype->componentSet.HasRelationship(ids[remainIdx]))
                    {
                        skip = true;
                    }
                }

                if(!skip)
                {
                    node->archetype = archetype;
                    ArchetypeLinkedList* newNode = ArchetypeLinkedList::Init(m_wAllocator);
                    node->next = newNode;
                    node = newNode;
                }
            }
        }

        sc.componentSet = std::move(componentSet);
        sc.archetypeList = head;

        if(m_systemStore.capacity == m_systemStore.count)
        {
            m_systemStore.Grow(m_wAllocator);
        }

        m_systemStore.Add(std::move(sc));
    }

    template<typename... Components, typename... FuncArgs>
    void World::Each(void (*func)(FuncArgs...))
    {
        EntityId ids[] = {ComponentTypeId<decay_t<Components>>::Id()...};
        uint32_t count = sizeof...(Components);

        ArchetypeLinkedList* node = ArchetypeLinkedList::Init(m_wAllocator);

        ArchetypeLinkedList* head = node;

        SystemCallback sc = CreateSystemCallback<Components..., FuncArgs...>(func);

        for(uint32_t idx = 0; idx < 1; idx++)
        {
            ComponentRecord& cr = m_componentIndex.GetValue(ids[idx]);

            for(uint32_t aIdx = 0; aIdx < cr.archetypeStore.count; aIdx++)
            {
                Archetype* archetype = cr.archetypeStore.store[aIdx];
                bool skip = false;
                assert(archetype);

                for(uint32_t remainIdx = 1; remainIdx < count; remainIdx++)
                {
                    //Archetype does not contain the same set of components
                    if(archetype->componentSet.Search(ids[remainIdx]) == -1)
                    {
                        skip = true;
                        break;
                    }
                }

                if(!skip)
                {
                    node->archetype = archetype;
                    ArchetypeLinkedList* newNode = ArchetypeLinkedList::Init(m_wAllocator);
                    node->next = newNode;
                    node = newNode;
                }
            }
        }

        void* componentsData[sizeof...(FuncArgs)];

        while(head->archetype)
        {
            Archetype* archetype = head->archetype;

            for(uint32_t row = 0; row < archetype->count; row++)
            {
                QueryIterator it;
                it.archetype = archetype;
                it.world = this;
                it.row = row;

                for(uint32_t idx = 0; idx < count; idx++)
                {
                    uint32_t componentId = ids[idx];

                    int32_t cIdx = archetype->componentSet.Search(componentId);

                    assert(cIdx != -1);

                    int32_t colIdx = archetype->componentMap[cIdx];

                    if(colIdx == -1)
                    {
                        componentsData[idx] = nullptr;
                    }
                    else
                    {
                        Column& col = archetype->columns[colIdx];
                        TypeInfo& ti = *col.typeInfo;
                        void* comData = OFFSET(col.data, ti.size * row);
                        componentsData[idx] = comData;
                    }
                }

                //EXECUTE
                sc.Execute(&it, componentsData);
            }
            ArchetypeLinkedList* freeNode = head;
            head = head->next;

            ArchetypeLinkedList::Free(m_wAllocator, freeNode);
        }
    }
}
