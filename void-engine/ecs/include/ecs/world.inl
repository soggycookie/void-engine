
namespace ECS
{
    template<typename T>
    TypeInfoBuilder<T> World::Component()
    {
        TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
        ti->size = sizeof(T);
        ti->alignment = alignof(T);
        ti->flags = (COMPONENT_TYPE | TYPE_HAS_DATA);
        ti->id = 0;
        //Entity e = CreateEntity(ComponentName<Component>::name, 0);
        //ti->id = e.GetFullId();

        assert(std::is_destructible_v<T>);
        assert(std::is_trivially_constructible_v<T>);

        TypeInfoBuilder<T> tiBuilder{*ti, this};

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


        tiBuilder.AddEvent(
            []()
            {
                std::cout << "Add component " << ComponentName<T>::name << std::endl;
            }
        );

        tiBuilder.RemoveEvent(
            []()
            {
                std::cout << "Remove component" << ComponentName<T>::name << std::endl;
            }
        );

        tiBuilder.SetEvent(
            [](void* dest)
            {
                std::cout << "Set component" << ComponentName<T>::name << std::endl;
            }
        );

        return tiBuilder;
    }

    template<typename T>
    TypeInfoBuilder<T> World::Tag()
    {
        TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
        ti->size = sizeof(T);
        ti->alignment = alignof(T);
        ti->flags = (TAG_TYPE);
        ti->id = 0;
        //Entity e = CreateEntity(ComponentName<Tag>::name, 0);

        assert(std::is_destructible_v<T>);
        assert(std::is_trivially_constructible_v<T>);
        assert(ti->size == 1 && "Tag can not have data");

        TypeInfoBuilder<T> tiBuilder{*ti, this};


        tiBuilder.AddEvent(
            []()
            {
                std::cout << "Add tag " << ComponentName<T>::name << std::endl;
            }
        );

        tiBuilder.RemoveEvent(
            []()
            {
                std::cout << "Remove tag" << ComponentName<T>::name << std::endl;
            }
        );

        tiBuilder.SetEvent(
            [](void* dest)
            {
                std::cout << "Set tag" << ComponentName<T>::name << std::endl;
            }
        );

        return tiBuilder;
    }

    template<typename T>
    TypeInfoBuilder<T> World::Relationship(bool isExclusive, bool isToggle)
    {
        TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
        ti->size = sizeof(T);
        ti->alignment = alignof(T);

        ti->flags = PAIR_TYPE;

        if(ti->size > 1 || isToggle)
        {
            ti->flags |= TYPE_HAS_DATA;
        }

        if(isExclusive)
        {
            ti->flags |= EXCLUSIVE_PAIR;
        }

        ti->id = 0;

        assert(std::is_destructible_v<T>);
        assert(std::is_trivially_constructible_v<T>);

        TypeInfoBuilder<T> tiBuilder{*ti, this};

        if(ti->size > 1)
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

        tiBuilder.AddEvent(
            []()
            {
                std::cout << "Add pair " << ComponentName<T>::name << std::endl;
            }
        );

        tiBuilder.RemoveEvent(
            []()
            {
                std::cout << "Remove pair" << ComponentName<T>::name << std::endl;
            }
        );

        tiBuilder.SetEvent(
            [](void* dest)
            {
                std::cout << "Set pair" << ComponentName<T>::name << std::endl;
            }
        );

        return tiBuilder;
    }

    template<typename T>
    void World::AddComponent(EntityId eId)
    {
        AddComponent(eId, ComponentTypeId<T>::id);
    }

    template<typename T>
    void World::AddRelationship(EntityId eId, EntityId targetId)
    {
        EntityId pairId = MakeRelationship(ComponentTypeId<T>::id, targetId);
        TypeInfo* pTi = m_typeInfos.GetValue(ComponentTypeId<T>::id);
        
        if(!m_componentIndex.ContainsKey(pairId))
        {
            TypeInfo* ti = new (m_wAllocator.Init(sizeof(TypeInfo))) TypeInfo();
            *ti = *pTi;
            ti->flags |= FULL_PAIR;
            ti->id = pairId;
            
            TypeInfoBuilder<T> builder{*ti, this};
            builder.Register();
        }

        EntityRecord* r = m_entityIndex.GetPageData(eId);

        assert(r);
        assert(!r->archetype->componentSet.Has(pairId));

        if(pTi->IsExclusive())
        {
            if(r->archetype->componentSet.HasPair(ComponentTypeId<T>::id))
            {
                return;
            }
        }

        Archetype* destArchetype = GetOrCreateArchetype_Add(r->archetype, pairId);

        if(destArchetype->count == destArchetype->capacity)
        {
            GrowArchetype(*destArchetype);
        }

        if(!r->archetype)
        {
            if(pTi->HasData())
            {
                void* dataAddr = OFFSET(destArchetype->columns[0].data, destArchetype->count * pTi->size);
                pTi->hook.ctor(dataAddr);
            }
        }
        else
        {
            SwapBack(*r);

            for(uint32_t i = 0; i < destArchetype->componentSet.count; i++)
            {
                //skip no data tag and pair
                int32_t destColIdx = destArchetype->componentMap[i];

                if(destColIdx == -1)
                {
                    continue;
                }

                Column& destCol = destArchetype->columns[destColIdx];
                TypeInfo& ti = *destCol.typeInfo;

                void* dest = OFFSET(destCol.data, ti.size * destArchetype->count);

                int32_t srcIndex = r->archetype->componentSet.Search(destArchetype->componentSet[i]);

                if(srcIndex == -1)
                {
                    ti.hook.ctor(dest);
                }
                else
                {
                    int32_t srcColIdx = r->archetype->componentMap[srcIndex];

                    if(srcColIdx == -1)
                    {
                        assert(0 && "Mismatch type");
                    }

                    Column& srcCol = r->archetype->columns[srcColIdx];
                    void* src = OFFSET(srcCol.data, ti.size * r->row);

                    if(ti.hook.moveCtor)
                    {
                        ti.hook.moveCtor(dest, src);
                    }
                    else if(ti.hook.copyCtor)
                    {
                        ti.hook.copyCtor(dest, src);
                    }
                    else
                    {
                        std::memcpy(dest, src, ti.size);
                    }

                    if(ti.hook.dtor)
                    {
                        ti.hook.dtor(src);
                    }
                }
            }

            --r->archetype->count;
        }

        destArchetype->entities[destArchetype->count] = eId;
        r->archetype = destArchetype;
        r->row = destArchetype->count;
        ++destArchetype->count;

        pTi->hook.onAdd();
    }

    template<typename T>
    void World::AddTag(EntityId eId)
    {
        EntityRecord* r = m_entityIndex.GetPageData(eId);

        assert(r);

        TypeInfo* pti = m_typeInfos.GetValue(ComponentTypeId<T>::id);

        assert(!r->archetype->componentSet.Has(ComponentTypeId<T>::id));

        if(pti->IsExclusive())
        {
            assert(!r->archetype->componentSet.HasPair(ComponentTypeId<T>::id));
        }

        Archetype* destArchetype = GetOrCreateArchetype_Add(r->archetype, ComponentTypeId<T>::id);

        if(destArchetype->count == destArchetype->capacity)
        {
            GrowArchetype(*destArchetype);
        }

        if(r->archetype)
        {
            SwapBack(*r);


            for(uint32_t i = 0; i < destArchetype->componentSet.count; i++)
            {
                //skip no data tag and pair
                int32_t destColIdx = destArchetype->componentMap[i];

                if(destColIdx == -1)
                {
                    continue;
                }

                Column& destCol = destArchetype->columns[destColIdx];
                TypeInfo& ti = *destCol.typeInfo;

                void* dest = OFFSET(destCol.data, ti.size * destArchetype->count);

                int32_t srcIndex = r->archetype->componentSet.Search(destArchetype->componentSet[i]);

                if(srcIndex == -1)
                {
                    ti.hook.ctor(dest);
                }
                else
                {
                    int32_t srcColIdx = r->archetype->componentMap[srcIndex];

                    if(srcColIdx == -1)
                    {
                        assert(0 && "Mismatch type");
                    }

                    Column& srcCol = r->archetype->columns[srcColIdx];
                    void* src = OFFSET(srcCol.data, ti.size * r->row);

                    if(ti.hook.moveCtor)
                    {
                        ti.hook.moveCtor(dest, src);
                    }
                    else if(ti.hook.copyCtor)
                    {
                        ti.hook.copyCtor(dest, src);
                    }
                    else
                    {
                        std::memcpy(dest, src, ti.size);
                    }

                    if(ti.hook.dtor)
                    {
                        ti.hook.dtor(src);
                    }
                }
            }

            --r->archetype->count;
        }

        destArchetype->entities[destArchetype->count] = eId;
        r->archetype = destArchetype;
        r->row = destArchetype->count;
        ++destArchetype->count;

        pti->hook.onAdd();
    }

    template<typename T>
    void World::RemoveComponent(EntityId eId)
    {
        RemoveComponent(eId, ComponentTypeId<T>::id);
    }

    template<typename T>
    void World::Set(EntityId eId, T&& c)
    {
        Set(eId, ComponentTypeId<decay_t<T>>::id, &c);
    }

    template<typename T>
    T& World::Get(EntityId eId)
    {
        void* data = Get(eId, ComponentTypeId<T>::id);

        T& component = PTR_CAST(data, CompTonent);

        return component;
    }

    template<typename... Components, typename... FuncArgs>
    void World::System(void (*func)(FuncArgs...))
    {
        EntityId ids[] = {ComponentTypeId<Components>::id...};
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
                       !archetype->componentSet.HasPair(ids[remainIdx]))
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
        EntityId ids[] = {ComponentTypeId<decay_t<Components>>::id...};
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