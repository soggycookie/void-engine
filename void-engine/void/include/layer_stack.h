#pragma once
#include "pch.h"
#include "layer.h"
#include "ds/dynamic_array.h"
#include "memory_system.h"

namespace VoidEngine
{
    class LayerStack
    {
    public:
        LayerStack(FreeListAllocator* allocator)
            : m_index(0), m_layers(allocator, 5)
        {
        }
        
        ~LayerStack() = default;

        void DestroyAll();

        void PushLayer(Layer* layer);
        void PushOverLay(Layer* layer);
        
        void PopLayer(Layer* layer);
        void PopOverLay(Layer* layer);

        DynamicArray<Layer*>::Iterator Begin();
        DynamicArray<Layer*>::Iterator End();

    private:
        DynamicArray<Layer*> m_layers;
        uint32_t m_index;
    };
}