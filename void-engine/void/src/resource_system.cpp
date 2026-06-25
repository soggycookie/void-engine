#include "resource_system.h"

namespace VoidEngine
{
const char *ResourceSystem::s_assetFolderName = "asset";
const char *ResourceSystem::s_resourceFolderName = "resource";
std::filesystem::path ResourceSystem::s_assetPath;
std::filesystem::path ResourceSystem::s_resourcePath;
ResourceRegistry ResourceSystem::s_resourceRegistry;

void ResourceSystem::LocateResourcePath()
{
    LOG_INFO("RESOURCE SYSTEM",
             "Current working directory: ", std::filesystem::current_path().string().c_str());
    if (std::filesystem::exists(s_assetFolderName) && std::filesystem::exists(s_resourceFolderName))
    {
        s_assetPath = std::filesystem::absolute(s_assetFolderName);
        s_resourcePath = std::filesystem::absolute(s_resourceFolderName);
    }
    else
    {
        LOG_ERROR("RESOURCE SYSTEM", "Failed to locate resource/asset directory!");
    }
}

void ResourceSystem::StartUp(FreeListAllocator *resourceLookUpAlloc, PoolAllocator *resourceAlloc)
{
    if (!resourceLookUpAlloc || !resourceAlloc)
    {
        assert(0 && "Allocators can not be null! [ResourceSystem]");
        return;
    }

    LocateResourcePath();
    s_resourceRegistry.Init(resourceLookUpAlloc, resourceAlloc);
}

#ifdef VOID_DEBUG
int32_t ResourceSystem::InspectRef(GUID guid) { return s_resourceRegistry.InspectRef(guid); }
#endif

void ResourceSystem::ShutDown() { s_resourceRegistry.DestroyAll(); }

} // namespace VoidEngine
