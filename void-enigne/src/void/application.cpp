#include "application.h"

#include "renderer.h"
#include "profiler.h"
#include "memory_system.h"
#include "resource_system.h"

#include "event/application_event.h"
#include "event/keyboard_event.h"
#include "event/mouse_event.h"

namespace VoidEngine
{
    bool Application::StartUp()
    {        
        MemorySystem::StartUp(m_config);
        
        m_isRunning = true;
        m_isResizing = false;
        
        m_window = Window::Create(WindowProperty(), [this](Event& e){OnEvent(e);});
        
        if(!m_window->Init())
        {
            return false;
        }

        void* layerStackAddr = MemorySystem::PersistantAllocator()->Alloc(sizeof(LayerStack));
        m_layerStack = new (layerStackAddr) LayerStack(MemorySystem::PersistantAllocator());

        void* gameLayerAddr = MemorySystem::PersistantAllocator()->Alloc(sizeof(GameLayer));
        GameLayer* gameLayer = new (gameLayerAddr) GameLayer();
        m_layerStack->PushLayer(gameLayer);

        ResourceSystem::StartUp(&MemorySystem::s_resourceLookUpAllocator, 
                                &MemorySystem::s_resourceAllocator,
                                &MemorySystem::s_resourceStreamAllocator);
        
        Renderer::StartUp(m_window);
        Renderer::SetGraphicAPI(GraphicAPI::D3D11);

        Profiler::StartUp(m_window);
        
        return true;
    }

    void Application::ShutDown()
    {
        std::cout << "window time: " << m_window->GetWindowTime() << std::endl;
        m_isRunning = false;
    }

    void Application::Update()
    {
        while(m_isRunning)
        {          
            m_window->Update();
            //SIMPLE_LOG(m_window->GetDeltaTime());
            Renderer::Update();
        }
    }

    //void Application::PushLayer(Layer* layer)
    //{
    //    m_layers.PushBack(layer);
    //}

    //void Application::PushOverLay(Layer* layer)
    //{
    //
    //}

    //TODO: create layer to dispatch event to lower layers
    void Application::OnEvent(Event& e)
    {
        switch(e.GetEventType())
        {
            case EventType::APP_CLOSED:
            {
                m_isRunning = false;
                break;
            }
            case EventType::APP_RESIZING:
            {
                break;
            }
            case EventType::APP_ENTER_RESIZE:
            {
                m_isResizing = true;
                break;
            }
            case EventType::APP_EXIT_RESIZE:
            {
                m_isResizing = false;
                break;
            }
            case EventType::KEY_PRESSED:
            {
                std::cout << "Key Pressed" << std::endl;
                break;
            }
            case EventType::KEY_RELEASED:
            {
                std::cout << "Key Released" << std::endl;
                break;
            }
            case EventType::MOUSE_MOVE:
            {
                MouseMovedEvent& mme = dynamic_cast<MouseMovedEvent&>(e);
                //std::cout << mme.GetX() << " " << mme.GetY() << std::endl;

                break;
            }
        }
    }
}