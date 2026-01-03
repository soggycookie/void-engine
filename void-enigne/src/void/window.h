#pragma once

#include "pch.h"
#include "void/event/event.h"
#include "void/global_persistant_allocator.h"

namespace VoidEngine
{
    using EventCallback = std::function<void(Event&)>;

    struct WindowProperty
    {
        std::string title = "void-engine";
        int width = 1280;
        int height = 720;
    };

    class Window
    {
    public:
        //define at platform-dependent layer
        static Window* Create(const WindowProperty& property, EventCallback func);

        Window(const WindowProperty& property, EventCallback func)
            : m_property(property), m_eventCallback(func), m_deltaTime(0),
              m_windowTime(0), m_isTimeStopped(false)
        {
            int32_t minWidth = 200;
            int32_t minHeight = 200;

            m_minWidth = minWidth;
            m_minHeight = minHeight;
        }

        virtual ~Window() = default;

        virtual void Update() = 0;

        virtual void BeginTimeElapse() = 0 ;
        virtual void EndTimeElapse(double& outPassedTime) = 0;
        virtual void* GetDisplayWindow() = 0;
        
        double GetDeltaTime() const
        {
            return m_deltaTime;
        }

        void DispatchEvent(Event& e){
            m_eventCallback(e);
        }

        double GetWindowTime() const
        {
            return m_windowTime;
        }

        ClientDimension GetDimension() const
        {
            ClientDimension dimension = {m_minWidth, m_minHeight};
            return dimension;
        }

        virtual bool Init() = 0;
        //virtual bool SetupRenderer()
        //{
        //    Renderer::SetWindow(this);
        //    return Renderer::SetGraphicAPI(GraphicAPI::D3D11);
        //}


    protected:
        //TODO: move this property to APP
        WindowProperty m_property;
        int32_t m_minWidth;
        int32_t m_minHeight;
        
        EventCallback m_eventCallback;

        //TODO: move this to another class
        double m_deltaTime;
        double m_windowTime;

        //TODO: handle case the app is paused when dragging
        bool m_isTimeStopped;
    };

}