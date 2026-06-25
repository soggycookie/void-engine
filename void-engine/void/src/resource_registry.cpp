#include "resource_registry.h"

namespace VoidEngine
{

void ResourceRegistry::Init(FreeListAllocator *resourceLookUpAlloc,
                            PoolAllocator *resourceAllocator)
{
    m_resourceAllocator = resourceAllocator;
    m_resourceLookUpAllocator = resourceLookUpAlloc;
    m_resourceLookUpTable = std::move(FlatHashMap<GUID, ResourceData>(m_resourceLookUpAllocator));
}

void ResourceRegistry::DestroyAll()
{
    for (auto it = m_resourceLookUpTable.Begin(); it != m_resourceLookUpTable.End(); it++)
    {
        if (it.IsValid())
        {
            auto resourceData = it.GetValue();
            switch (resourceData.type)
            {
            // case ResourceType::MESH:
            //{
            //     resourceRef.As<MeshResource>()->~MeshResource();
            //     break;
            // }
            case ResourceType::SHADER:
            {
                // resourceData.As<ShaderResource>()->~ShaderResource();
                break;
            }
            case ResourceType::MATERIAL:
            {
                // resourceData.As<MaterialResource>()->~MaterialResource();
                break;
            }
            default:
            {
                SIMPLE_LOG("[ResourceCache] Destroy unknown resource type!");
                break;
            }
            }

            m_resourceAllocator->Free(resourceData.rsc);
            m_resourceLookUpTable.Remove(it.GetKey());
        }
    }
    m_resourceAllocator->Clear();
}

void ResourceRegistry::DestroyUnused()
{
    for (auto it = m_resourceLookUpTable.Begin(); it != m_resourceLookUpTable.End(); it++)
    {
        if (it.IsValid())
        {
            auto resourceData = it.GetValue();

            if (resourceData.ref == 0)
            {
                switch (resourceData.type)
                {
                // case ResourceType::MESH:
                //{
                //     resourceRef.As<MeshResource>()->~MeshResource();
                //     break;
                // }
                case ResourceType::SHADER:
                {
                    // resourceData.As<ShaderResource>()->~ShaderResource();
                    break;
                }
                case ResourceType::MATERIAL:
                {
                    // resourceData.As<MaterialResource>()->~MaterialResource();
                    break;
                }
                default:
                {
                    assert(0 && "Destroy unknown resource type! [ResourceCache]");
                    break;
                }
                }

                m_resourceAllocator->Free(resourceData.rsc);
                m_resourceLookUpTable.Remove(it.GetKey());
            }
        }
    }
}

#ifdef VOID_DEBUG
int32_t ResourceRegistry::InspectRef(GUID guid)
{
    if (m_resourceLookUpTable.ContainsKey(guid))
    {
        auto &resourceRef = m_resourceLookUpTable[guid];
        return resourceRef.ref;
    }


    return -1;
}
#endif
} // namespace VoidEngine
