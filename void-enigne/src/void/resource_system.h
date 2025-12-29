#pragma once
#include "resource_cache.h"
#include "allocator/free_list_allocator.h"
#include "allocator/pool_allocator.h"
#include "resource_type_traits.h"

namespace VoidEngine
{
    class ResourceSystem
    {
    public:
        static void Load(const ResourceGUID& guid, ResourceType type);
        
        static ResourceGUID GenerateGUID()
        {
            return 1;
        }

        template<typename T, typename... Args>
        static T* Create(ResourceGUID guid, ResourceType type, Args&&... args)
        {
            static_assert(
                ResourceTypeTraits<T>::type == type,
                "Type and template type mismatch! [ResourceSystem.Create]"
            );

            if(type == ResourceType::UNKNOWN)
            {
                SIMPLE_LOG("Resource type can not be unknown! [ResourceSystem.Create]");
            }

            return ResourceCache::Create(guid, type, args);  
        }

        template<typename T>
        static void Release(T& rsrc)
        {
            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "T is not a registered resource type [ResourceSystem.Destroy]"
            );

            ResourceCache::Release<T>(rsrc.GUID());
        }
        
        template<typename T>
        static T* Acquire(const ResourceGUID& guid)
        {
            auto resourceRef = ResourceCache::Acquire(guid);

            static_assert(
                ResourceTypeTraits<T>::type == resourceRef.type,
                "Type and template type mismatch! [ResourceSystem.Create]"
            );

            return resourceRef.As<T>();   
        }

        template<typename T>
        static void Destroy(ResourceGUID guid)
        {

            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "T is not a registered resource type [ResourceSystem.Destroy]"
            );

            ResourceCache::Destroy(guid);
        }

    private:
        friend class Application;

        static void StartUp(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAlloc, FreeListAllocator* streamAlloc);
        static void ShutDown();


    };
}