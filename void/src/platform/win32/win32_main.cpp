#define NOMINMAX
#include <windows.h>
#include <winuser.h>

// belong to game layer
// but platform dependent
#include "application.h"
#include "logger/log.h"

#ifdef VOID_DEBUG

int main()
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(178);
    using namespace VoidEngine;

    Application *app = nullptr;
    try
    {
        app = new Application();

        if (app->startup())
        {
            app->update();
        }
        app->shutdown();
    }
    catch (const std::runtime_error &e)
    {
        if (app)
        {
            Logger::ShowErrorDialog(app->get_window()->get_window_handle(), "Error", e.what());
        }
        else
        {
            Logger::ShowErrorDialog(nullptr, "Error", e.what());
        }
    }
    catch (...)
    {
    }

    if (app)
    {
        delete app;
    }

    FLUSH_LOG();
    return 0;
}

#else

#ifdef VOID_WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    using namespace VoidEngine;
    Application *app = new Application();

    if (app->StartUp())
    {
        app->Update();
    }
    app->ShutDown();
    delete app;

    return 0;
}
#else
#endif // VOID_WIN32

#endif // VOID_DEBUG
