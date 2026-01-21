#pragma once
#include "pch.h"
#include "event/event.h"

namespace VoidEngine
{
    class Layer
    {
    public:
        Layer() = default;

        virtual ~Layer() = default;
        virtual void OnInit() = 0;
        virtual void OnDetach() = 0;
        virtual void OnAttach() = 0;
        
        virtual void OnUpdate(double dt) = 0;
        virtual void OnEvent(const Event& e) = 0;

    };


}