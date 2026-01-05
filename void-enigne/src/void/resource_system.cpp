#include "resource_system.h"
#include "resource_cache.h"
#include "resource.h"
#include "renderer.h"

namespace VoidEngine
{


    void ResourceSystem::StartUp(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAlloc)
    {
        if(!resourceLookUpAlloc || !resourceAlloc )
        {
            assert(0 && "Allocators can not be null! [ResourceSystem]");
            return;
        }

        ResourceCache::Init(resourceLookUpAlloc, resourceAlloc);
    }

#ifdef VOID_DEBUG
    int32_t ResourceSystem::InspectRef(ResourceGUID guid){
        return ResourceCache::InspectRef(guid);
    }
#endif

    void ResourceSystem::ShutDown()
    {
        ResourceCache::DestroyAll();
    }

    void ResourceSystem::LoadBundle(const std::wstring_view file)
    {
        
    }
}
