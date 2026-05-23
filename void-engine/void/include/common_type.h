#pragma once

struct ClientDimension
{
public:
    int width;
    int height;
};

enum class GraphicAPI
{
    UNKNOWN,
    D3D11,
    VULKAN
};

using ResourceGUID = size_t;
