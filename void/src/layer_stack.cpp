#include "layer_stack.h"
#include "logger/log.h"

namespace VoidEngine
{

void LayerStack::destroy()
{
    for (uint64_t idx = 0; idx < count; ++idx)
    {
        layers[idx]->on_detach();
    }
}

void LayerStack::push_layer(Layer *layer)
{
    ENGINE_ASSERT(layer, "Layer must not be null!");
    ENGINE_ASSERT(count < MAX_LAYERS, "Layer stack is full!");
    // layers live in [0, index), overlays in [index, count): shift overlays up to make room
    for (uint32_t idx = count; idx > index; --idx)
    {
        layers[idx] = layers[idx - 1];
    }
    layers[index] = layer;
    ++index;
    ++count;
    layer->on_attach();
}

void LayerStack::push_overlay(Layer *layer)
{
    ENGINE_ASSERT(layer, "Overlayer must not be null!");
    ENGINE_ASSERT(count < MAX_LAYERS, "Layer stack is full!");
    layers[count] = layer;
    ++count;
    layer->on_attach();
}

// void LayerStack::pop_layer(Layer *layer)
// {
//     auto it = layers.find(layer);
//
//     if (it != layers.End())
//     {
//         layers.remove(it);
//         m_index--;
//         layer->OnDetach();
//     }
// }
//
// void LayerStack::PopOverLay(Layer *layer)
// {
//     auto it = layers.find(layer);
//
//     if (it != layers.End())
//     {
//         layers.remove(it);
//         layer->OnDetach();
//     }
// }
} // namespace VoidEngine
