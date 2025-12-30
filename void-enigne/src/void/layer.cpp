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
        Renderer::Update();
    }

    void GameLayer::OnDetach()
    {
        SIMPLE_LOG("detach!");
    }

    void GameLayer::OnEvent(const Event& e)
    {
        switch(e.GetEventType())
        {

            case EventType::KEY_PRESSED:
            {
                std::cout << "Key Pressed Game Layer" << std::endl;
                break;
            }
            case EventType::KEY_RELEASED:
            {
                std::cout << "Key Released Game Layer" << std::endl;
                break;
            }
        }
    }
}