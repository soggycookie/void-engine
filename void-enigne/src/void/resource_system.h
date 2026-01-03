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
        
        static ResourceGUID GenerateGUID()
        {
            static size_t guid = 0;
            return guid++;
        }

        template<typename T, typename... Args>
        static T* Create(ResourceGUID guid, Args&&... args)
        {
            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "Type and template type mismatch! [ResourceSystem.Create]"
            );

            return ResourceCache::Create<T>(guid, std::forward<Args>(args)...);  
        }

        template<typename T>
        static void Release(T& rsrc)
        {
            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "T is not a registered resource type [ResourceSystem.Release]"
            );

            ResourceCache::Release<T>(rsrc.GUID());
        }
        
        template<typename T>
        static T* Acquire(const ResourceGUID& guid)
        {

            static_assert(
                ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                "T is not a registered resource type [ResourceSystem.Acquire]"
            );

            auto resourceRef = ResourceCache::Acquire(guid);

            assert(
                ResourceTypeTraits<T>::type == resourceRef.type &&
                "Resource type and template type mismatch! [ResourceSystem.Acquire]"
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

        static void Load(const std::wstring_view file);
    private:
        friend class Application;

        static void StartUp(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAlloc, FreeListAllocator* streamAlloc);
        static void ShutDown();


    };
}