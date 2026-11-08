#pragma once
#include "layer.h"
#include "logger/log.h"

namespace VoidEngine
{

constexpr static uint32_t MAX_LAYERS = 8;

class LayerStack
{
public:
    LayerStack() : index(0), count(0), layers() {}

    void init();
    void destroy();

    void push_layer(Layer *layer);
    void push_overlay(Layer *layer);
    uint32_t get_count() const { return count; }
    // void pop_layer(Layer *layer);
    // void pop_overlay(Layer *layer);

    Layer *&operator[](uint32_t idx)
    {
        ENGINE_ASSERT(idx < count, "Index out of bound!");
        return layers[idx];
    }

private:
    Layer *layers[MAX_LAYERS];
    uint32_t index;
    uint32_t count;
};
} // namespace VoidEngine
