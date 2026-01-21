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

        void* layerStackAddr = MemorySystem::GeneralAllocator()->Alloc(sizeof(LayerStack));
        m_layerStack = new (layerStackAddr) LayerStack(MemorySystem::GeneralAllocator());

        void* gameLayerAddr = MemorySystem::GeneralAllocator()->Alloc(sizeof(GameLayer));
        m_gameLayer = new (gameLayerAddr) GameLayer();
        m_layerStack->PushLayer(m_gameLayer);
        
        //layer should before this because events dispatch to app 
        // then dispatch to layers, which should already exist
        if(!m_window->Init())
        {
            return false;
        }

        ResourceSystem::StartUp(&MemorySystem::s_resourceLookUpAllocator, 
                                &MemorySystem::s_resourceAllocator);
        
        Renderer::StartUp(m_window);
        Renderer::SetGraphicAPI(GraphicAPI::D3D11);

        Profiler::StartUp(m_window);
        

        return true;
    }

    void Application::ShutDown()
    {
        m_layerStack->DestroyAll();
        MemorySystem::GeneralAllocator()->Free(m_layerStack);

        Profiler::ShutDown();
        Renderer::ShutDown();
        ResourceSystem::ShutDown();

        MemorySystem::ShutDown();
        std::cout << "window time: " << m_window->GetWindowTime() << std::endl;
        m_isRunning = false;
    }

    void Application::Update()
    {
        for(auto it = m_layerStack->End(); it != m_layerStack->Begin();)
        {
            (*(--it))->OnInit();
        }

        while(m_isRunning)
        {          
            m_window->Update();
            //SIMPLE_LOG(m_window->GetDeltaTime());

            for(auto it = m_layerStack->End(); it != m_layerStack->Begin();)
            {
                (*(--it))->OnUpdate(m_window->GetDeltaTime());
            }
        }
    }

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
        }

        for(auto it = m_layerStack->End(); it != m_layerStack->Begin();)
        {
            (*(--it))->OnEvent(e);
        }
    }
}