#include "layer.h"
#include "renderer.h"

namespace VoidEngine
{
    void GameLayer::OnAttach()
    {
        SIMPLE_LOG("attach!");
    }

    void GameLayer::OnUpdate(double dt)
    {

    }

    void GameLayer::OnDeteach()
    {
        SIMPLE_LOG("detach!");
    }

    void GameLayer::OnEvent(const Event& e)
    {

        SIMPLE_LOG("event event!");
    }
}