#include "SDL.h"
#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include <stdio.h>

#include "SDL_syswm.h"

static void* sdlNativeWindowHandle(SDL_Window* _window)
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(_window, &wmi))
    {
        return NULL;
    }
    return wmi.info.win.window;
}


#ifdef main
#undef main
#endif

int main(void)
{
    SDL_Window* window = nullptr;

    int width = 800;
    int height = 600;

    printf("initializing SDL\n");
    SDL_Init(SDL_INIT_GAMECONTROLLER);

    printf("create SDL window\n");
    window =
        SDL_CreateWindow("bgfx", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    printf("initializing bgfx\n");

    // Call bgfx::renderFrame before bgfx::init to signal to bgfx not to create a render thread.
    // Most graphics APIs must be used on the same thread that created the window.
    bgfx::renderFrame();

    bgfx::Init init;
    init.platformData.nwh = sdlNativeWindowHandle(window);
    init.resolution.width = width;
    init.resolution.height = height;
    init.resolution.reset = BGFX_RESET_VSYNC;
    bgfx::init(init);


    const bgfx::ViewId kClearView = 0;
    bgfx::setViewClear(kClearView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020ff, 1.0f, 0);
    bgfx::setViewRect(kClearView, 0, 0, width, height);

    float time = 0.0f;
    SDL_Event event;
    bool isRunning = true;
    while (isRunning)
    {

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                isRunning = false;
                break;
            }

            // event.type
        }

        bgfx::touch(kClearView);

        bgfx::dbgTextClear();
        uint16_t x = uint16_t(fabsf(cosf(time)) * 20.0f);
        printf("step %d\n", x);

        bgfx::dbgTextPrintf(x, 0, 0x0f, "Hello world");
        bgfx::setDebug(BGFX_DEBUG_TEXT);

        time += 0.1f;

        bgfx::frame();
        SDL_Delay(30);
    }

    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}