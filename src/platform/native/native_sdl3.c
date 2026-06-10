#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "../../../subprojects/clay/renderers/SDL3/clay_renderer_SDL3.c"

#include "app.h"
#include "command_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *user_data) {
    TTF_Font **fonts = (TTF_Font **)user_data;
    TTF_Font *font = fonts[config->fontId];
    int width = 0;
    int height = 0;

    if (!font) {
        SDL_Log("No font loaded for id %u", config->fontId);
        return (Clay_Dimensions){0};
    }

    TTF_SetFontSize(font, config->fontSize);
    if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
        SDL_Log("TTF_GetStringSize failed: %s", SDL_GetError());
    }

    return (Clay_Dimensions){ (float)width, (float)height };
}

static const char *FindDefaultFontPath(void) {
    const char *paths[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], 16);
        if (font) {
            TTF_CloseFont(font);
            return paths[i];
        }
    }

    return NULL;
}

static ProgTP_AppAction ActionFromKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_N: return PROGTP_APP_ACTION_NEXT;
        case SDLK_P: return PROGTP_APP_ACTION_PREVIOUS;
        case SDLK_A: return PROGTP_APP_ACTION_ADD_SAMPLE;
        case SDLK_U: return PROGTP_APP_ACTION_UPDATE_SELECTED;
        case SDLK_R: return PROGTP_APP_ACTION_REMOVE_SELECTED;
        case SDLK_1: return PROGTP_APP_ACTION_MODULE_1;
        case SDLK_2: return PROGTP_APP_ACTION_MODULE_2;
        case SDLK_3: return PROGTP_APP_ACTION_MODULE_3;
        case SDLK_4: return PROGTP_APP_ACTION_MODULE_4;
        case SDLK_5: return PROGTP_APP_ACTION_MODULE_5;
        case SDLK_6: return PROGTP_APP_ACTION_MODULE_6;
        case SDLK_7: return PROGTP_APP_ACTION_MODULE_7;
        case SDLK_8: return PROGTP_APP_ACTION_MODULE_8;
        case SDLK_C: return PROGTP_APP_ACTION_SEARCH_CODE;
        case SDLK_I: return PROGTP_APP_ACTION_SEARCH_IP;
        case SDLK_M: return PROGTP_APP_ACTION_SEARCH_MAC;
        case SDLK_BACKSPACE: return PROGTP_APP_ACTION_INPUT_BACKSPACE;
        case SDLK_RETURN: return PROGTP_APP_ACTION_INPUT_SUBMIT;
        case SDLK_ESCAPE: return PROGTP_APP_ACTION_FORM_CANCEL;
        case SDLK_TAB: return PROGTP_APP_ACTION_FORM_NEXT_FIELD;
        case SDLK_W: return PROGTP_APP_ACTION_SAVE;
        case SDLK_L: return PROGTP_APP_ACTION_LOAD;
        default: break;
    }
    return PROGTP_APP_ACTION_NONE;
}

int main(int argc, char **argv) {
    const char *remote_url = ProgTP_FindRemoteUrl(argc, argv);
    ProgTP_CommandResult command_result;
    char command_error[256] = {0};
    if (!ProgTP_LoadCommandResult(argc, argv, &command_result, command_error, sizeof(command_error))) {
        SDL_Log("Falling back to local mode: %s", command_error);
        ProgTP_RunLocalCommand(&command_result);
    }
    char command_label[192];
    ProgTP_FormatCommandResultLabel(&command_result, command_label, sizeof(command_label));
    ProgTP_AppState app_state;
    ProgTP_AppInit(&app_state, remote_url == NULL, "equipamentos.dat");
    if (remote_url) {
        char inventory_error[256] = {0};
        if (ProgTP_LoadRemoteInventory(remote_url, &app_state.inventory, inventory_error, sizeof(inventory_error))) {
            ProgTP_AppUseLoadedInventory(&app_state, "Loaded inventory from HTTP server");
            snprintf(command_label, sizeof(command_label), "%s", "Mode: connected to server | inventory stored on server PC");
        } else {
            char status[320];
            snprintf(status, sizeof(status), "HTTP inventory load failed: %s", inventory_error);
            ProgTP_AppSetStatus(&app_state, status);
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = NULL;
    Clay_SDL3RendererData renderer_data = {0};
    if (!SDL_CreateWindowAndRenderer("ProgTP Clay SDL3", 900, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer_data.renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 1;
    }
    SDL_StartTextInput(window);

    const char *font_path = FindDefaultFontPath();
    if (!font_path) {
        SDL_Log("Could not find a default TrueType font");
        return 1;
    }
    renderer_data.textEngine = TTF_CreateRendererTextEngine(renderer_data.renderer);
    if (!renderer_data.textEngine) {
        SDL_Log("TTF_CreateRendererTextEngine failed: %s", SDL_GetError());
        return 1;
    }
    renderer_data.fonts = SDL_calloc(1, sizeof(*renderer_data.fonts));
    if (!renderer_data.fonts) {
        SDL_Log("Could not allocate font array");
        return 1;
    }
    renderer_data.fonts[0] = TTF_OpenFont(font_path, 24);
    if (!renderer_data.fonts[0]) {
        SDL_Log("TTF_OpenFont failed: %s", SDL_GetError());
        SDL_free((void *)renderer_data.fonts);
        TTF_DestroyRendererTextEngine(renderer_data.textEngine);
        SDL_DestroyRenderer(renderer_data.renderer);
        SDL_DestroyWindow(window);
        ProgTP_AppDestroy(&app_state);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    uint64_t clay_memory_size = Clay_MinMemorySize();
    Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, malloc(clay_memory_size));
    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(renderer_data.renderer, &width, &height);
    Clay_Initialize(clay_arena, (Clay_Dimensions){ (float)width, (float)height }, (Clay_ErrorHandler){ ProgTP_HandleClayError, NULL });
    Clay_SetMeasureTextFunction(MeasureText, (void *)renderer_data.fonts);

    bool running = true;
    uint64_t last_save_attempt_version = 0;
    uint64_t previous_ticks = SDL_GetTicks();
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                SDL_GetRenderOutputSize(renderer_data.renderer, &width, &height);
                Clay_SetLayoutDimensions((Clay_Dimensions){ (float)width, (float)height });
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                Clay_UpdateScrollContainers(true, (Clay_Vector2){ event.wheel.x, event.wheel.y }, 0.01f);
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                ProgTP_AppAction action = ActionFromKey(event.key.key);
                if ((app_state.input_mode != PROGTP_APP_INPUT_NONE || ProgTP_AppModalActive(&app_state)) &&
                    action != PROGTP_APP_ACTION_INPUT_BACKSPACE &&
                    action != PROGTP_APP_ACTION_INPUT_SUBMIT &&
                    event.key.key >= 32u &&
                    event.key.key <= 126u) {
                    ProgTP_AppHandleTextInput(&app_state, (uint32_t)event.key.key);
                } else {
                    ProgTP_AppHandleAction(&app_state, action);
                }
            }
        }

        float mouse_x = 0;
        float mouse_y = 0;
        uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        SDL_RenderCoordinatesFromWindow(renderer_data.renderer, mouse_x, mouse_y, &mouse_x, &mouse_y);
        Clay_SetPointerState((Clay_Vector2){ mouse_x, mouse_y }, (buttons & SDL_BUTTON_LMASK) != 0);

        uint64_t ticks = SDL_GetTicks();
        float delta_time = (float)(ticks - previous_ticks) / 1000.0f;
        previous_ticks = ticks;

        ProgTP_ConnectivityRequest connectivity_request;
        if (ProgTP_AppTakeConnectivityRequest(&app_state, &connectivity_request)) {
            ProgTP_ConnectivityResult connectivity_result;
            char connectivity_error[256] = {0};
            bool connectivity_ok = remote_url
                ? ProgTP_RunRemoteConnectivity(
                    remote_url,
                    &connectivity_request,
                    &connectivity_result,
                    connectivity_error,
                    sizeof(connectivity_error))
                : ProgTP_RunLocalConnectivity(
                    &app_state.inventory,
                    &connectivity_request,
                    &connectivity_result,
                    connectivity_error,
                    sizeof(connectivity_error));
            if (!connectivity_ok) {
                ProgTP_AppFailConnectivityRequest(&app_state, connectivity_error);
            } else if (remote_url && connectivity_result.inventory_changed) {
                char inventory_error[256] = {0};
                if (ProgTP_LoadRemoteInventory(
                        remote_url,
                        &app_state.inventory,
                        inventory_error,
                        sizeof(inventory_error))) {
                    ProgTP_AppUseLoadedInventory(&app_state, "Reloaded inventory after server command");
                    ProgTP_AppCompleteConnectivityRequest(&app_state, &connectivity_result, false);
                } else {
                    ProgTP_AppFailConnectivityRequest(&app_state, inventory_error);
                }
            } else {
                ProgTP_AppCompleteConnectivityRequest(
                    &app_state,
                    &connectivity_result,
                    remote_url == NULL);
            }
        }

        Clay_RenderCommandArray commands = ProgTP_AppBuildLayout(&app_state, command_label, delta_time);

        if (remote_url && ProgTP_AppInventoryDirty(&app_state)) {
            uint64_t version = ProgTP_AppInventoryVersion(&app_state);
            if (version != last_save_attempt_version) {
                char save_error[256] = {0};
                last_save_attempt_version = version;
                if (ProgTP_SaveRemoteInventory(remote_url, &app_state.inventory, save_error, sizeof(save_error))) {
                    ProgTP_AppMarkInventoryClean(&app_state);
                    ProgTP_AppSetStatus(&app_state, "Saved inventory to HTTP server");
                } else {
                    char status[320];
                    snprintf(status, sizeof(status), "HTTP save failed: %s", save_error);
                    ProgTP_AppSetStatus(&app_state, status);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer_data.renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer_data.renderer);
        SDL_Clay_RenderClayCommands(&renderer_data, &commands);
        SDL_RenderPresent(renderer_data.renderer);
    }

    if (renderer_data.fonts) {
        TTF_CloseFont(renderer_data.fonts[0]);
        SDL_free((void *)renderer_data.fonts);
    }
    if (renderer_data.textEngine) {
        TTF_DestroyRendererTextEngine(renderer_data.textEngine);
    }
    SDL_DestroyRenderer(renderer_data.renderer);
    SDL_DestroyWindow(window);
    free(clay_arena.memory);
    ProgTP_AppDestroy(&app_state);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
