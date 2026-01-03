#pragma once
#include "layer.h"

namespace VoidEngine
{
    class GameLayer : public Layer
    {
    public:
        GameLayer():
            m_gameTime(0), m_initResource(false)
        {
        }

        void OnDetach() override;
        void OnAttach() override;
        void OnUpdate(double dt) override;
        void OnEvent(const Event& e) override;

    private:
        size_t m_gameTime;
        bool m_initResource;
    };
}