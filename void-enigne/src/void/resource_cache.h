#pragma once
#include "pch.h"
#include "ds/flat_hash_map.h"
#include "allocator/pool_allocator.h"
#include "resource.h"
#include "resource_type_traits.h"

namespace VoidEngine
{


#define NULL_RESOURCE_REF {nullptr, ResourceType::UNKNOWN, 0}

    struct ResourceRef
    {
        void* rsrc;
        ResourceType type;
        int32_t ref;

        template<typename T>
        T* As()
        {
            return static_cast<T*>(rsrc);
        }
    };

    class ResourceCache
    {
    private:
        friend class ResourceSystem;

        static void Init(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAllocator);
        
        static ResourceRef Acquire(ResourceGUID guid);

        template<typename T, typename... Args>
        static T* Create(ResourceGUID guid, int32_t ref, Args&&... args)
        {
            if(s_resourceLookUpTable.ContainsKey(guid))
            {
#ifdef VOID_DEBUG
                assert(0 && "Failed to create resource! [ResourceCache]");        
#endif
                return nullptr;
            }
            else
            {
                void* resourceAddr = s_resourceAllocator->Alloc(0);
                T* rsrc = new (resourceAddr) T(guid, std::forward<Args>(args)...);
                
#ifdef VOID_DEBUG
                std::cout << "[ResourceCache] Inserted resource! GUID: " << guid << " , type: " << typeid(T).name()  << std::endl;
#endif
                s_resourceLookUpTable.Insert(guid, {rsrc, ResourceTypeTraits<T>::type, ref});
                
                return rsrc;
            }
        }
        
        template<typename T>
        static void Release(ResourceGUID guid)
        {
            if(s_resourceLookUpTable.ContainsKey(guid))
            {
                auto& resourceRef = s_resourceLookUpTable[guid];
                resourceRef.ref--;

                if(resourceRef.ref == 0)
                {
                    T* rsrc = resourceRef.As<T>();
                    rsrc->~T();
                    s_resourceLookUpTable.Remove(guid);
                    s_resourceAllocator->Free(rsrc);                
                }
            }
            else
            {
                SIMPLE_LOG("Key not existed [ResourceCache]");
            }
        }

        template<typename T>        
        static void Destroy(ResourceGUID guid)
        {
            if(s_resourceLookUpTable.ContainsKey(guid))
            {
                T* rsrc = s_resourceLookUpTable[guid].As<T>();
                rsrc.~T();
                s_resourceLookUpTable.Remove(guid);
                s_resourceAllocator->Free(rsrc);
            }
            else
            {
                SIMPLE_LOG("Key not existed [ResourceCache]");
            }
        }

        static void DestroyAll();

        static void DestroyUnused();

#ifdef VOID_DEBUG
        static int32_t InspectRef(ResourceGUID guid);
#endif

    private:
        static FlatHashMap<ResourceGUID, ResourceRef> s_resourceLookUpTable;
        static FreeListAllocator* s_resourceLookUpAllocator;
        static PoolAllocator* s_resourceAllocator;
    };


    //class ResourceStream
    //{
    //private:
    //    friend class ResourceSystem;

    //    static void Init(FreeListAllocator* allocator);
    //    static void LoadResourceFile(const ResourceGUID& file, void** outAddr);    
    //    static void Unload(void* resource);

    //priva ic FreeListAllocator* s_resourceStreamAllocator;
    //};
}
