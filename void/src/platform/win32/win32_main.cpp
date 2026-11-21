#define NOMINMAX
#include <windows.h>
#include <winuser.h>

#include "application.h"
#include "logger/log.h"

#ifdef VOID_DEBUG

int main()
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(178);
    using namespace VoidEngine;

    Application *app = nullptr;
    app = new Application();

    if (app->startup())
    {
        app->update();
    }
    app->shutdown();

    FLUSH_LOG();
    delete app;

    return 0;
}

#else

#ifdef VOID_WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    using namespace VoidEngine;

    Application *app = nullptr;
    app = new Application();

    if (app->startup())
    {
        app->update();
    }
    app->shutdown();

    FLUSH_LOG();
    delete app;

    return 0;
}
#else
#endif // VOID_WIN32

#endif // VOID_DEBUG
