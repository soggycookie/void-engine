#pragma once
#include "resource.h"

namespace VoidEngine
{
    template<typename T>
    struct ResourceTypeTraits 
    {
        static const ResourceType type = ResourceType::UNKNOWN;
    };

    template<> struct ResourceTypeTraits<MeshResource> 
    {
        static const ResourceType type = ResourceType::MESH;
    };

    template<> struct ResourceTypeTraits<ShaderResource> 
    {
        static const ResourceType type = ResourceType::SHADER;
    };

    template<> struct ResourceTypeTraits<MaterialResource> 
    {
        static const ResourceType type = ResourceType::MATERIAL;
    };
}