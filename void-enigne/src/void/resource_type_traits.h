#pragma once
#include "resource.h"

namespace VoidEngine
{
    template<typename T>
    struct ResourceTypeTraits {
        static constexpr ResourceType type = ResourceType::UNKNOWN;
    };

    template<> struct ResourceTypeTraits<MeshResource> {
        static constexpr ResourceType type = ResourceType::MESH;
    };
}