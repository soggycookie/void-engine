#pragma once
#include "pch.h"

namespace VoidEngine
{

#define DEFAULT_PERSISTANT_ALLOC_SIZE       KB(4)
#define DEFAULT_PER_FRAME_ALLOC_SIZE        MB(8)
#define DEFAULT_RESOURCE_LOOKUP_ALLOC_SIZE  MB(2)
#define DEFAULT_RESOURCE_STREAM_ALLOC_SIZE  MB(128)
#define DEFAULT_RESOURCE_ALLOC_SIZE         MB(16)
#define DEFAULT_RESOURCE_CHUNK_SIZE         128

    struct EngineConfig
    {
        size_t persistantAllocatorSize      = DEFAULT_PERSISTANT_ALLOC_SIZE;
        size_t perFrameAllocatorSize        = DEFAULT_PER_FRAME_ALLOC_SIZE;
        size_t resourceLookUpAllocatorSize  = DEFAULT_RESOURCE_LOOKUP_ALLOC_SIZE;
        size_t resourceStreamAllocatorSize  = DEFAULT_RESOURCE_STREAM_ALLOC_SIZE;
        size_t resourceAllocatorSize        = DEFAULT_RESOURCE_ALLOC_SIZE;
        size_t resourceChunkSize            = DEFAULT_RESOURCE_CHUNK_SIZE;
        size_t resourceAlignment            = DEFAULT_ALIGNMENT;
    };

}