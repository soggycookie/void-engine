#pragma once
#include "log.h"
#include "pch.h"
#include "renderer.h"
#include "resource.h"
#include "resource_registry.h"

#include "allocator/free_list_allocator.h"
#include "allocator/pool_allocator.h"
#include <filesystem>

namespace VoidEngine
{



class ResourceSystem
{
public:
     //template <typename T, typename... Args>
     //static T *Create(GUID guid, Args &&...args)
     //{
     //    static_assert(ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
     //                  "Type and template type mismatch!");
    
     //    return s_resourceRegistry.Create<T>(guid, 1, std::forward<Args>(args)...);
     //}

    template <typename T>
    static ResourceHandle<T> Acquire(const GUID &guid)
    {

        static_assert(ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                      "T is not a registered resource type");

        if (s_resourceRegistry.Contains(guid))
        {
            LOG_ERROR("RESOURCE SYSTEM", "Resource GUID %d does not exist!", guid);
            return ResourceHandle<T>(kInvalidGUID);
        }

        auto &resourceData = s_resourceRegistry.Get(guid);
        ++resourceData.ref;

        if (!resourceData.isLoaded)
        {
            Reload<T>(resourceData);
        }

        LOG_ASSERT(ResourceTypeTraits<T>::type == resourceData.type,
                   "Resource type and template type mismatch!");

        return ResourceHandle<T>(guid);
    }

    template <typename T>
    static void Release(GUID guid)
    {
        static_assert(ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                      "T is not a registered resource type");

        if (s_resourceRegistry.Contains(guid))
        {
            LOG_ERROR("RESOURCE SYSTEM", "Resource GUID %d does not exist!", guid);
            //return ResourceHandle<T>(kInvalidGUID);
        }

        auto &resourceData = s_resourceRegistry.Get(guid);
        --resourceData.ref;

        LOG_ASSERT(ResourceTypeTraits<T>::type == resourceData.type,
                   "Resource type and template type mismatch!");

        if (resourceData.ref == 0)
        {
            s_resourceRegistry.Unload<T>(resourceData);
        }
    }

    /// <summary>
    /// This will wipe the resource out of the table, even if there are others refering to it
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <param name="guid"></param>

    template <typename T>
    static void Destroy(GUID guid)
    {

        static_assert(ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                      "T is not a registered resource type [ResourceSystem.Destroy]");

        s_resourceRegistry.Destroy<T>(guid);
    }

    template <typename T>
    static ResourceHandle<T> Load(const std::wstring_view file)
    {
        static_assert(ResourceTypeTraits<T>::type != ResourceType::UNKNOWN,
                      "T is not a registered resource type [ResourceSystem.Destroy]");

        std::filesystem::path p;

        if (std::filesystem::exists(s_assetPath / file))
        {
            p = s_assetPath / file;
        }
        else if (std::filesystem::exists(s_resourcePath / file))
        {
            p = s_resourcePath / file;
        }
        else
        {
            LOG_ERROR("RESOURCE SYSTEM", "%s does not exist", file);
            return nullptr;
        }

        switch (ResourceTypeTraits<T>::type) {}
    }

#ifdef VOID_DEBUG
    static int32_t InspectRef(GUID guid);
#endif

    static void LoadBundle(const std::wstring_view file);

private:
    friend class Application;

    static void StartUp(FreeListAllocator *resourceLookUpAlloc, PoolAllocator *resourceAlloc);
    static void ShutDown();
    static void LocateResourcePath();

    template <typename T>
    static void Reload(ResourceData &resourceData)
    {
    }

private:
    static ResourceRegistry s_resourceRegistry;

    static const char *s_assetFolderName;
    static const char *s_resourceFolderName;

    static std::filesystem::path s_assetPath;
    static std::filesystem::path s_resourcePath;
};

template <typename T>
ResourceHandle<T>::ResourceHandle(const ResourceHandle<T> &handle)
{
    m_guid = handle.m_guid;
    ResourceSystem::Acquire<T>(m_guid);
}

template <typename T>
T *ResourceHandle<T>::Get()
{
    return nullptr;
}

template <typename T>
ResourceHandle<T>::ResourceHandle(GUID guid) : m_guid(guid)
{
    // ResourceSystem::Acquire<T>(m_guid);
}
} // namespace VoidEngine
