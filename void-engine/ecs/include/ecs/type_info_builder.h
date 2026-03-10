#pragma once
#include "ecs_type.h"
#include <type_traits>

namespace ECS
{
    class World;

    template<typename T = void>
    struct TypeInfoBuilder
    {
        TypeInfo ti;
        World* world;
        EntityId first;
        EntityId second;
        
        TypeInfoBuilder(World* world) 
            : ti(), world(world), second(0), first(0)
        {
            if constexpr (!std::is_void_v<T>) { 
                ti.size = sizeof(T);
                ti.alignment = alignof(T);
            }

            ti.eId = 0;
        }
        
        TypeInfoBuilder<T>& Ctor(CtorHook ctor);
        
        TypeInfoBuilder<T>& CopyCtor(CopyCtorHook cctor);

        TypeInfoBuilder<T>& MoveCtor(MoveCtorHook mctor);

        TypeInfoBuilder<T>& Dtor(DtorHook dtor);

        TypeInfoBuilder<T>& AddEvent(AddEventHook e);

        TypeInfoBuilder<T>& RemoveEvent(RemoveEventHook e);

        TypeInfoBuilder<T>& SetEvent(SetEventHook e);

        TypeInfoBuilder<T>& Id(EntityId id);

        void Register();
    
    private:
        friend class World;

        TypeInfoBuilder<T>& Exclusive();
        
        TypeInfoBuilder<T>& HasData();

        TypeInfoBuilder<T>& Component();

        TypeInfoBuilder<T>& Tag();
        
        TypeInfoBuilder<T>& Relation();
        
        TypeInfoBuilder<T>& Relationship(EntityId relationId,EntityId targetId);
        
        TypeInfoBuilder<T>& Relationship(EntityId targetId);
    };
}
