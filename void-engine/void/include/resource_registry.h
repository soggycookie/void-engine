#pragma once
#include "pch.h"
#include "allocator/pool_allocator.h"
#include "ds/flat_hash_map.h"
#include "resource.h"

namespace VoidEngine
{

class ResourceRegistry
{
private:
    friend class ResourceSystem;

    ResourceRegistry() = default;

    void Init(FreeListAllocator *resourceLookUpAlloc, PoolAllocator *resourceAllocator);

    ResourceData &Get(GUID guid) { return m_resourceLookUpTable[guid]; }

    bool Contains(GUID guid) { return m_resourceLookUpTable.ContainsKey(guid); }

    template <typename T, typename... Args>
    ResourceData &Create(GUID guid, const char *file, Args &&...args)
    {
        if (m_resourceLookUpTable.ContainsKey(guid))
        {

             auto rsc = m_resourceLookUpTable[guid];
            
             if (rsc.isLoaded)
             {
                 return rsc.As<T>();
             }
        }
        else
        {
            void *resourceAddr = m_resourceAllocator->Alloc(0);
            T *rsc = new (resourceAddr) T(guid, std::forward<Args>(args)...);

            m_resourceLookUpTable.Insert(guid, {
                                                   .rsc = rsc,
                                                   .file = file,
                                                   .guid = guid,
                                                   .ref = 0,
                                                   .type = ResourceTypeTraits<T>::type,
                                                   .isLoaded = true,
                                               });
        }

        return m_resourceLookUpTable[guid];
    }

    template <typename T>
    void Destroy(GUID guid)
    {
        if (m_resourceLookUpTable.ContainsKey(guid))
        {
            T *rsrc = m_resourceLookUpTable[guid].As<T>();
            rsrc.~T();
            m_resourceLookUpTable.Remove(guid);
            m_resourceAllocator->Free(rsrc);
        }
        else
        {
            LOG_ERROR("RESOURCE SYSTEM", "Resource does not exist");
        }
    }

    template <typename T>
    void Unload(ResourceData &resourceData)
    {
        static_assert(ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                      "T is not a registered resource type");

        T *rsc = resourceData.As<T>();
        rsc->~T();
        resourceData.isLoaded = false;
        m_resourceAllocator->Free(rsc);
    }

    void DestroyAll();

    void DestroyUnused();

#ifdef VOID_DEBUG
    int32_t InspectRef(GUID guid);
#endif

private:
    FlatHashMap<GUID, ResourceData> m_resourceLookUpTable;
    FreeListAllocator *m_resourceLookUpAllocator;
    PoolAllocator *m_resourceAllocator;
};

} // namespace VoidEngine
