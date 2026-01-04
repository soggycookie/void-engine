#include "game_layer.h"
#include "renderer.h"
#include "resource_system.h"
namespace VoidEngine
{
    void GameLayer::OnAttach()
    {
        SIMPLE_LOG("attach!");
    }

    void GameLayer::OnUpdate(double dt)
    {
        if(!m_initResource)
        {
            MeshResource* mesh = ResourceSystem::Create<MeshResource>(123, false);
            auto shader = ResourceSystem::Load<ShaderResource>(L"src//asset//shader//square_demo.hlsl");
            auto material = ResourceSystem::Create<MaterialResource>(ResourceSystem::GenerateGUID(), shader->GetGUID());
            
            SIMPLE_LOG(ResourceSystem::InspectRef(shader->GetGUID()));
            m_initResource = true;
        }
        else
        {
            Renderer::Update();
        }
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