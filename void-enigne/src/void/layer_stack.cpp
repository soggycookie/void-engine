#include "layer_stack.h"

namespace VoidEngine
{
    void LayerStack::DestroyAll()
    {
        for(auto it = m_layers.Begin(); it != m_layers.End(); it++)
        {
            (*it)->OnDetach();
            MemorySystem::GeneralAllocator()->Free(*it);
        }
    }

    void LayerStack::PushLayer(Layer* layer)
    {
        m_layers.Push(m_layers.Begin() + m_index, layer);
        m_index++;
        layer->OnAttach();
    }
    
    void LayerStack::PushOverLay(Layer* layer)
    {
        m_layers.PushBack(layer);
        layer->OnAttach();
    }
    
    void LayerStack::PopLayer(Layer* layer)
    {
        auto it = m_layers.Find(layer);

        if(it != m_layers.End())
        {
            m_layers.Remove(it);
            m_index--;
            layer->OnDetach();
        }
    }

    void LayerStack::PopOverLay(Layer* layer)
    {
        auto it = m_layers.Find(layer);

        if(it != m_layers.End())
        {
            m_layers.Remove(it);
            layer->OnDetach();
        }
    }

    DynamicArray<Layer*>::Iterator LayerStack::Begin()
    {
        return m_layers.Begin();
    }
    
    DynamicArray<Layer*>::Iterator LayerStack::End()
    {
        return m_layers.End();
    }
}