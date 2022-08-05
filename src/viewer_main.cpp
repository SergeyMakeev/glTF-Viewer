#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_syswm.h"

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"

#include "meshoptimizer.h"

#include "gltf_loader.h"

#include <stdio.h>

const bgfx::ViewId kViewId = 0;

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


static void renderFrame(float deltaTime)
{
    static float time = 0.0f;

    bgfx::dbgTextClear();
    uint16_t x = uint16_t(fabsf(cosf(time)) * 20.0f);

    bgfx::dbgTextPrintf(x, 0, 0x0f, "Hello World");
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    time += (deltaTime * 4.0f);
}

int main(void)
{
    SDL_Window* window = nullptr;

    int width = 800;
    int height = 600;

    printf("Initializing SDL\n");
    SDL_SetMainReady();
    SDL_Init(SDL_INIT_GAMECONTROLLER);

    printf("Create SDL window\n");
    window =
        SDL_CreateWindow("bgfx", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    printf("Initializing bgfx\n");

    // Call bgfx::renderFrame before bgfx::init to signal to bgfx not to create a render thread.
    // Most graphics APIs must be used on the same thread that created the window.
    bgfx::renderFrame();

    bgfx::Init init;
    init.platformData.nwh = sdlNativeWindowHandle(window);
    init.resolution.width = width;
    init.resolution.height = height;
    init.resolution.reset = BGFX_RESET_VSYNC;
    bgfx::init(init);

    bgfx::setViewClear(kViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020ff, 1.0f, 0);
    bgfx::setViewRect(kViewId, 0, 0, width, height);

    gltf::loadFile("../data/doctor/scene.gltf");
    //gltf::loadFile("../data/SimpleSkin.gltf");
    //gltf::loadFile("../data/Duck.glb");

    Uint64 prevFrameTime = SDL_GetPerformanceCounter();

    SDL_Event event;
    bool isRunning = true;
    while (isRunning)
    {
        while (SDL_PollEvent(&event))
        {
            int nwidth, nheight;
            SDL_GetWindowSize(window, &nwidth, &nheight);
            if (nwidth != width || nheight != height)
            {
                printf("Reset\n");
                width = nwidth;
                height = nheight;
                bgfx::reset((uint32_t)width, (uint32_t)height, BGFX_RESET_VSYNC);
                bgfx::setViewRect(kViewId, 0, 0, width, height);
            }

            switch (event.type)
            {
            case SDL_QUIT:
                isRunning = false;
                break;
            }
        }

        bgfx::touch(kViewId);

        Uint64 timeNow = SDL_GetPerformanceCounter();
        Uint64 ticksPassed = (timeNow - prevFrameTime);
        prevFrameTime = timeNow;
        double deltaTime = double(ticksPassed) / double(SDL_GetPerformanceFrequency());
        renderFrame(float(deltaTime));
        bgfx::frame();
    }

    bgfx::shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}