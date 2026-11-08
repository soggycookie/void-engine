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
    layers[index] = layer;
    ++index;
    ++count;
    layer->on_attach();
}

void LayerStack::push_overlay(Layer *layer)
{
    ENGINE_ASSERT(layer, "Overlayer must not be null!");
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
