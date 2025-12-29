#include "profiler.h"
#include "window.h"

namespace VoidEngine
{
    Window* Profiler::s_window = nullptr;

    void Profiler::BeginTimeElapse()
    {
        if(!s_window)
        {
            return;
        }

        s_window->BeginTimeElapse();
    }

    void Profiler::EndTimeElapse(double& elapsedTime)
    {
        if(!s_window)
        {
            return;
        }

        s_window->EndTimeElapse(elapsedTime);
    }

    void Profiler::StartUp(Window* window)
    {
        if(!window)
        {
            assert(0 && "Window is null! [Renderer]");
        }

        s_window = window;
    }
}