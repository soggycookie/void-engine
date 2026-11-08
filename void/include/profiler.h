#pragma once

namespace VoidEngine
{
    class Window;

    class Profiler
    {
    public:

        static void BeginTimeElapse();
        static void EndTimeElapse(double& elapsedTime);

    private:
        friend class Application;

        static void StartUp(Window* window);
        static void ShutDown() {}

    private:
        static Window* s_window;
    };
}
