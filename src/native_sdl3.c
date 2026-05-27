#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "app.h"
#include "clay_renderer_sdl3_basic.h"
#include "command_client.h"

#include <stdbool.h>
#include <stdlib.h>

static Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *user_data) {
    TTF_Font **fonts = user_data;
    TTF_Font *font = fonts[config->fontId];
    int width = 0;
    int height = 0;

    TTF_SetFontSize(font, config->fontSize);
    if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
        SDL_Log("TTF_GetStringSize failed: %s", SDL_GetError());
    }

    return (Clay_Dimensions){ (float)width, (float)height };
}

static TTF_Font *OpenDefaultFont(void) {
    const char *paths[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], 24);
        if (font) {
            return font;
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    ProgTP_CommandResult command_result;
    char command_error[256] = {0};
    if (!ProgTP_LoadCommandResult(argc, argv, &command_result, command_error, sizeof(command_error))) {
        SDL_Log("Falling back to local mode: %s", command_error);
        ProgTP_RunLocalCommand(&command_result);
    }
    char command_label[192];
    ProgTP_FormatCommandResultLabel(&command_result, command_label, sizeof(command_label));

    if (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = NULL;
    Clay_SDL3RendererData renderer_data = {0};
    if (!SDL_CreateWindowAndRenderer("ProgTP Clay SDL3", 900, 600, SDL_WINDOW_RESIZABLE, &window, &renderer_data.renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 1;
    }

    renderer_data.textEngine = TTF_CreateRendererTextEngine(renderer_data.renderer);
    renderer_data.fonts = SDL_calloc(1, sizeof(TTF_Font *));
    if (!renderer_data.textEngine || !renderer_data.fonts) {
        SDL_Log("SDL text setup failed: %s", SDL_GetError());
        return 1;
    }

    renderer_data.fonts[0] = OpenDefaultFont();
    if (!renderer_data.fonts[0]) {
        SDL_Log("Could not find a default TrueType font");
        return 1;
    }

    uint64_t clay_memory_size = Clay_MinMemorySize();
    Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, malloc(clay_memory_size));
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    Clay_Initialize(clay_arena, (Clay_Dimensions){ (float)width, (float)height }, (Clay_ErrorHandler){ ProgTP_HandleClayError, NULL });
    Clay_SetMeasureTextFunction(MeasureText, renderer_data.fonts);

    bool running = true;
    uint64_t previous_ticks = SDL_GetTicks();
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                Clay_SetLayoutDimensions((Clay_Dimensions){ (float)event.window.data1, (float)event.window.data2 });
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                Clay_UpdateScrollContainers(true, (Clay_Vector2){ event.wheel.x, event.wheel.y }, 0.01f);
            }
        }

        float mouse_x = 0;
        float mouse_y = 0;
        uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        Clay_SetPointerState((Clay_Vector2){ mouse_x, mouse_y }, (buttons & SDL_BUTTON_LMASK) != 0);

        uint64_t ticks = SDL_GetTicks();
        float delta_time = (float)(ticks - previous_ticks) / 1000.0f;
        previous_ticks = ticks;

        Clay_RenderCommandArray commands = ProgTP_BuildHelloWorldLayout(command_label, delta_time);

        SDL_SetRenderDrawColor(renderer_data.renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer_data.renderer);
        SDL_Clay_RenderClayCommands(&renderer_data, &commands);
        SDL_RenderPresent(renderer_data.renderer);
    }

    TTF_CloseFont(renderer_data.fonts[0]);
    SDL_free(renderer_data.fonts);
    TTF_DestroyRendererTextEngine(renderer_data.textEngine);
    SDL_DestroyRenderer(renderer_data.renderer);
    SDL_DestroyWindow(window);
    free(clay_arena.memory);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
