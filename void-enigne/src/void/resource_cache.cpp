#include "resource_cache.h"

namespace VoidEngine
{
    PoolAllocator* ResourceCache::s_resourceAllocator = nullptr;
    FreeListAllocator* ResourceCache::s_resourceLookUpAllocator = nullptr;
    FlatHashMap<ResourceGUID, ResourceRef> ResourceCache::s_resourceLookUpTable;


    ResourceRef ResourceCache::Acquire(ResourceGUID guid)
    {
        if(s_resourceLookUpTable.ContainsKey(guid))
        {
            auto& resourceRef = s_resourceLookUpTable[guid];
            resourceRef.ref++;

            return resourceRef;
        }
        
        assert(0 && "Failed to acquire resource! [ResourceCache]");
        return NULL_RESOURCE_REF;
    }

    void ResourceCache::Init(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAllocator)
    {
        SIMPLE_LOG("Resource cache Init");
        s_resourceAllocator = resourceAllocator;
        s_resourceLookUpAllocator = resourceLookUpAlloc;
        s_resourceLookUpTable = std::move(FlatHashMap<ResourceGUID, ResourceRef>(s_resourceLookUpAllocator));
    }

    void ResourceCache::DestroyAll()
    {
        for(auto it = s_resourceLookUpTable.Begin(); it != s_resourceLookUpTable.End(); it++)
        {
            if(it.IsValid())
            {
                auto resourceRef = it.GetValue();
                switch(resourceRef.type)
                {
                    case ResourceType::MESH:
                    {
                        resourceRef.As<MeshResource>()->~MeshResource();
                        break;
                    }
                    case ResourceType::SHADER:
                    {
                        resourceRef.As<ShaderResource>()->~ShaderResource();
                        break;
                    }
                    case ResourceType::MATERIAL:
                    {
                        resourceRef.As<MaterialResource>()->~MaterialResource();
                        break;
                    }
                    default:
                    {
                        SIMPLE_LOG("[ResourceCache] Destroy unknown resource type!");
                        break;
                    }
                }

                s_resourceAllocator->Free(resourceRef.rsrc);
                s_resourceLookUpTable.Remove(it.GetKey());
            }
        }
        s_resourceAllocator->Clear();
    }
    
    void ResourceCache::DestroyUnused()
    {
        for(auto it = s_resourceLookUpTable.Begin(); it != s_resourceLookUpTable.End(); it++)
        {
            if(it.IsValid())
            {
                auto resourceRef = it.GetValue();

                if(resourceRef.ref == 0)
                {
                    switch(resourceRef.type)
                    {
                        case ResourceType::MESH:
                        {
                            resourceRef.As<MeshResource>()->~MeshResource();
                            break;
                        }
                        case ResourceType::SHADER:
                        {
                            resourceRef.As<ShaderResource>()->~ShaderResource();
                            break;
                        }
                        case ResourceType::MATERIAL:
                        {
                            resourceRef.As<MaterialResource>()->~MaterialResource();
                            break;
                        }
                        default:
                        {
                            assert(0 && "Destroy unknown resource type! [ResourceCache]");
                            break;
                        }
                    }

                    s_resourceAllocator->Free(resourceRef.rsrc);
                    s_resourceLookUpTable.Remove(it.GetKey());
                }
            }
        }
    }

#ifdef VOID_DEBUG
        int32_t ResourceCache::InspectRef(ResourceGUID guid)
        {
            if(s_resourceLookUpTable.ContainsKey(guid))
            {
                auto& resourceRef = s_resourceLookUpTable[guid];
                return resourceRef.ref;
            }
            else
            {
                SIMPLE_LOG("Key not existed [ResourceCache]");
            }            
            return -1;
        }
#endif
}