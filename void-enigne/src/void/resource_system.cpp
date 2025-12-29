#include "resource_system.h"
#include "resource_cache.h"

namespace VoidEngine
{


    void ResourceSystem::StartUp(FreeListAllocator* resourceLookUpAlloc, PoolAllocator* resourceAlloc, FreeListAllocator* streamAlloc)
    {
        if(!resourceLookUpAlloc || !resourceAlloc || !streamAlloc)
        {
            assert(0 && "Allocators can not be null! [ResourceSystem]");
            return;
        }

        //ResourceStream::Init(streamAlloc);
        ResourceCache::Init(resourceLookUpAlloc, resourceAlloc);
    }

    void ResourceSystem::ShutDown()
    {
        //Free resource
        ResourceCache::DestroyAll();
    }

    void ResourceSystem::Load(const ResourceGUID& guid, ResourceType type)
    {
        //void* streamAddr;
        //ResourceStream::LoadResourceFile(guid, &streamAddr);
        ///ResourceCache::Insert(guid, streamAddr, type);
        //m_resourceAllocator.

    }
}
