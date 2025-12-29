#pragma once
#include "void/window.h"
#include <windows.h>

namespace VoidEngine
{

#define HR(hr) if(FAILED(hr)) { return false; }

    class Win32Window : public Window
    {
    public:
        Win32Window(const WindowProperty& property, EventCallback func) 
            : Window(property, func),
              m_currCount(0), m_prevCount(0)
        {
            QueryPerformanceFrequency(&m_countsPerSec);
        }

        void Update() override;

        //move this to profiler
        void BeginTimeElapse() override;
        void EndTimeElapse(double& outPassedTime) override;

        void* GetDisplayWindow() override;

    private:
        bool Init() override;
        //bool SetupRenderer() override;

        void SetUpScene();

    private:
        HWND m_windowHandle;
        LARGE_INTEGER m_countsPerSec;
        
        int64_t m_currCount;
        int64_t m_prevCount;
        
        int64_t m_stopWatchCurrCount;
        int64_t m_stopWatchPrevCount;


    };

}