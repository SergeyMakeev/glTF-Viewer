#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_syswm.h"

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"

#include "meshoptimizer.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

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

static void loadMesh(const cgltf_node* node, const cgltf_data* data)
{
    if (node->has_translation)
    {
        printf("t: %3.2f, %3.2f, %3.2f\n", node->translation[0], node->translation[1], node->translation[2]);
    }

    if (node->has_rotation)
    {
        printf("r: %3.2f, %3.2f, %3.2f, %3.2f\n", node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
    }

    if (node->has_scale)
    {
        printf("s: %3.2f, %3.2f, %3.2f\n", node->scale[0], node->scale[1], node->scale[2]);
    }

    if (node->has_matrix)
    {
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[0], node->matrix[1], node->matrix[2], node->matrix[3]);
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[4], node->matrix[5], node->matrix[6], node->matrix[7]);
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[8], node->matrix[9], node->matrix[10], node->matrix[11]);
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[12], node->matrix[13], node->matrix[14], node->matrix[15]);       
    }

    if (node->skin)
    {
        const cgltf_skin* skin = node->skin;
        printf("Has skin\n");
        
    }

    //
    const cgltf_mesh* mesh = node->mesh;
    size_t numDrawCalls = mesh->primitives_count;
    for (size_t dc = 0; dc < numDrawCalls; dc++)
    {
        const cgltf_primitive* drawCall = &mesh->primitives[dc];
        printf("Num blend shapes: %d\n", int(drawCall->targets_count));

        if (drawCall->type != cgltf_primitive_type_triangles || !drawCall->indices)
        {
            continue;
        }

        if (drawCall->indices->component_type != cgltf_component_type_r_16u)
        {
            continue;
        }

        // read index buffer
        const cgltf_accessor* ind = drawCall->indices;
        char* buffer = (char*)ind->buffer_view->buffer->data;
        size_t offset = (ind->offset + ind->buffer_view->offset);
        buffer = buffer + offset * ind->stride;
        printf("num indices = %d\n", int(ind->count));
        for (size_t n = 0; n < ind->count; n++)
        {
            uint16_t* data = (uint16_t*)buffer;
            uint16_t index = *data;
            //printf("i: %d\n", index);
            buffer += ind->stride;
        }

        printf("num vertex attributes = %d\n", int(drawCall->attributes_count));
        for (size_t attrIndex = 0; attrIndex < drawCall->attributes_count; attrIndex++)
        {
            const cgltf_attribute* attr = &drawCall->attributes[attrIndex];
            const cgltf_accessor* vertexAttrData = attr->data;

            printf("[%d] = %d\n", int(attrIndex), int(vertexAttrData->count));
        }
    }
}

static void loadFile(const char* filePath)
{
    //
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filePath, &data);
    if (result != cgltf_result_success)
    {
        printf("Can't parse glTF file\n");
        exit(0);
    }

    result = cgltf_load_buffers(&options, data, filePath);
    if (result != cgltf_result_success)
    {
        printf("Can't load glTF buffers\n");
        exit(0);
    }

    /*
        result = cgltf_validate(data);
        if (result != cgltf_result_success)
        {
            printf("Can't validate glTF file\n");
            exit(0);
        }
    */

    if (data->meshes_count == 0 || !data->meshes)
    {
        printf("No meshes\n");
        exit(0);
    }

    for (size_t nodeIndex = 0; nodeIndex < data->nodes_count; nodeIndex++)
    {
        const cgltf_node* node = &data->nodes[nodeIndex];
        if (node->mesh)
        {
            loadMesh(node, data);
        }
    }

    cgltf_free(data);
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

    loadFile("../data/SimpleSkin.gltf");
    //loadFile("../data/Duck.glb");

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