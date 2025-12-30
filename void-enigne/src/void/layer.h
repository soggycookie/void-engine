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

        virtual void OnDetach() = 0;
        virtual void OnUpdate(double dt) = 0;
        virtual void OnAttach() = 0;
        virtual void OnEvent(const Event& e) = 0;

    };

    class GameLayer : public Layer
    {
    public:
        GameLayer() = default;

        void OnDetach() override;
        void OnAttach() override;
        void OnUpdate(double dt) override;
        void OnEvent(const Event& e) override;
    };
}