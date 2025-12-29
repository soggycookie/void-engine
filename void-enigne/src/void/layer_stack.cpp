#include "layer_stack.h"

namespace VoidEngine
{
    LayerStack::~LayerStack()
    {
        for(auto it = m_layers.Begin(); it != m_layers.End(); it++)
        {
            
        }
    }

    void LayerStack::PushLayer(Layer* layer)
    {
        m_layers.Insert(m_layers.Begin() + m_index, layer);
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
            layer->OnDeteach();
        }
    }

    void LayerStack::PopOverLay(Layer* layer)
    {
        auto it = m_layers.Find(layer);

        if(it != m_layers.End())
        {
            m_layers.Remove(it);
            layer->OnDeteach();
        }
    }
}