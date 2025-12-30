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
}