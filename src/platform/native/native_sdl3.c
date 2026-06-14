#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "../../../subprojects/clay/renderers/SDL3/clay_renderer_SDL3.c"

#include "app.h"
#include "command_client.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Thread *thread;
    atomic_bool done;
    bool active;
    bool remote;
    bool ok;
    bool inventory_available;
    bool inventory_changed_locally;
    char remote_url[512];
    char error[256];
    ProgTP_EquipmentInventory inventory;
    ProgTP_ConnectivityRequest request;
    ProgTP_ConnectivityResult result;
} ConnectivityJob;

typedef struct {
    SDL_Thread *thread;
    atomic_bool done;
    bool active;
    bool remote;
    bool ok;
    char remote_url[512];
    char sensor_input_path[512];
    char error[256];
    ProgTP_SensorStore store;
    ProgTP_SensorImportResult result;
} SensorJob;

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
        case SDLK_G: return PROGTP_APP_ACTION_SENSOR_IMPORT;
        case SDLK_O: return PROGTP_APP_ACTION_SENSOR_CHOOSE_FILE;
        case SDLK_I: return PROGTP_APP_ACTION_SEARCH_IP;
        case SDLK_M: return PROGTP_APP_ACTION_SEARCH_MAC;
        case SDLK_D: return PROGTP_APP_ACTION_INCIDENT_DELETE;
        case SDLK_E: return PROGTP_APP_ACTION_INCIDENT_EDIT;
        case SDLK_S: return PROGTP_APP_ACTION_INCIDENT_START;
        case SDLK_T: return PROGTP_APP_ACTION_INCIDENT_AUTO_IMPORT;
        case SDLK_Q: return PROGTP_APP_ACTION_INCIDENT_ADD;
        case SDLK_Z: return PROGTP_APP_ACTION_CONFIG_UNDO;
        case SDLK_Y: return PROGTP_APP_ACTION_CONFIG_REDO;
        case SDLK_F: return PROGTP_APP_ACTION_CONFIG_FILTER_ALL;
        case SDLK_B: return PROGTP_APP_ACTION_CONFIG_IMPORT;
        case SDLK_X: return PROGTP_APP_ACTION_CONFIG_DELETE;
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

static int ConnectivityThreadMain(void *user_data) {
    ConnectivityJob *job = user_data;
    job->ok = job->remote
        ? ProgTP_RunRemoteConnectivity(
            job->remote_url,
            &job->request,
            &job->result,
            job->error,
            sizeof(job->error))
        : ProgTP_RunLocalConnectivity(
            &job->inventory,
            &job->request,
            &job->result,
            job->error,
            sizeof(job->error));
    if (job->ok && job->remote && job->result.inventory_changed) {
        job->ok = ProgTP_LoadRemoteInventory(job->remote_url, &job->inventory, job->error, sizeof(job->error));
        job->inventory_available = job->ok;
    } else if (job->ok && !job->remote && job->result.inventory_changed) {
        job->inventory_available = true;
        job->inventory_changed_locally = true;
    }
    atomic_store_explicit(&job->done, true, memory_order_release);
    return 0;
}

static int SensorThreadMain(void *user_data) {
    SensorJob *job = user_data;
    const char *path = job->sensor_input_path[0] != '\0' ? job->sensor_input_path : NULL;
    job->ok = job->remote
        ? ProgTP_RunRemoteSensorImport(
            job->remote_url,
            &job->store,
            &job->result,
            path,
            job->error,
            sizeof(job->error))
        : ProgTP_RunLocalSensorImport(&job->store, &job->result, path, job->error, sizeof(job->error));
    atomic_store_explicit(&job->done, true, memory_order_release);
    return 0;
}

static bool CopyInventory(ProgTP_EquipmentInventory *destination, const ProgTP_EquipmentInventory *source, char *error, size_t error_size) {
    ProgTP_EquipmentInventoryInit(destination);
    return ProgTP_EquipmentInventoryReplace(
        destination,
        source->array.items,
        source->array.length,
        source->next_code,
        error,
        error_size);
}

static void TransferInventory(ProgTP_EquipmentInventory *destination, ProgTP_EquipmentInventory *source) {
    ProgTP_EquipmentInventoryDestroy(destination);
    *destination = *source;
    memset(source, 0, sizeof(*source));
}

static void StartConnectivityJob(
    ConnectivityJob *job,
    const char *remote_url,
    const ProgTP_AppState *app_state,
    const ProgTP_ConnectivityRequest *request) {
    memset(job, 0, sizeof(*job));
    atomic_init(&job->done, false);
    job->active = true;
    job->remote = remote_url != NULL;
    if (remote_url) {
        snprintf(job->remote_url, sizeof(job->remote_url), "%s", remote_url);
    }
    job->request = *request;
    if (!CopyInventory(&job->inventory, &app_state->inventory, job->error, sizeof(job->error))) {
        job->ok = false;
        atomic_store(&job->done, true);
        return;
    }
    job->thread = SDL_CreateThread(ConnectivityThreadMain, "progtp-connectivity", job);
    if (!job->thread) {
        snprintf(job->error, sizeof(job->error), "could not start connectivity worker");
        job->ok = false;
        atomic_store(&job->done, true);
    }
}

static void FinishConnectivityJob(ConnectivityJob *job, ProgTP_AppState *app_state) {
    if (job->thread) {
        int ignored = 0;
        SDL_WaitThread(job->thread, &ignored);
        job->thread = NULL;
    }
    if (!job->ok) {
        ProgTP_AppFailConnectivityRequest(app_state, job->error);
    } else {
        if (job->inventory_available) {
            TransferInventory(&app_state->inventory, &job->inventory);
            if (job->remote) {
                ProgTP_AppUseLoadedInventory(app_state, "Reloaded inventory after server command");
            }
        }
        ProgTP_AppCompleteConnectivityRequest(app_state, &job->result, job->inventory_changed_locally);
    }
    ProgTP_EquipmentInventoryDestroy(&job->inventory);
    memset(job, 0, sizeof(*job));
}

static void StartSensorJob(SensorJob *job, const char *remote_url, const ProgTP_AppState *app_state) {
    memset(job, 0, sizeof(*job));
    atomic_init(&job->done, false);
    job->active = true;
    job->remote = remote_url != NULL;
    if (remote_url) {
        snprintf(job->remote_url, sizeof(job->remote_url), "%s", remote_url);
    }
    snprintf(job->sensor_input_path, sizeof(job->sensor_input_path), "%s", app_state->sensor_input_path);
    if (!ProgTP_SensorStoreCopy(&job->store, &app_state->sensors, job->error, sizeof(job->error))) {
        job->ok = false;
        atomic_store(&job->done, true);
        return;
    }
    job->thread = SDL_CreateThread(SensorThreadMain, "progtp-sensors", job);
    if (!job->thread) {
        snprintf(job->error, sizeof(job->error), "could not start sensor worker");
        job->ok = false;
        atomic_store(&job->done, true);
    }
}

static void FinishSensorJob(SensorJob *job, ProgTP_AppState *app_state) {
    if (job->thread) {
        int ignored = 0;
        SDL_WaitThread(job->thread, &ignored);
        job->thread = NULL;
    }
    if (job->ok) {
        ProgTP_AppCompleteSensorImport(app_state, &job->result, &job->store);
    } else {
        ProgTP_AppFailSensorImport(app_state, job->error);
    }
    ProgTP_SensorStoreDestroy(&job->store);
    memset(job, 0, sizeof(*job));
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
        ProgTP_SensorStore remote_sensors;
        ProgTP_SensorStoreInit(&remote_sensors);
        char sensor_error[256] = {0};
        if (ProgTP_LoadRemoteSensors(remote_url, &remote_sensors, sensor_error, sizeof(sensor_error))) {
            ProgTP_AppUseLoadedSensors(&app_state, &remote_sensors, "Loaded sensors from HTTP server");
        } else {
            char status[320];
            snprintf(status, sizeof(status), "HTTP sensor load failed: %s", sensor_error);
            ProgTP_AppSetStatus(&app_state, status);
        }
        ProgTP_SensorStoreDestroy(&remote_sensors);
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
    ConnectivityJob connectivity_job = {0};
    SensorJob sensor_job = {0};
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
            } else if (event.type == SDL_EVENT_TEXT_INPUT) {
                if (app_state.input_mode != PROGTP_APP_INPUT_NONE || ProgTP_AppModalActive(&app_state)) {
                    const char *text = event.text.text;
                    if (text && (unsigned char)text[0] >= 32u) {
                        int codepoint = (unsigned char)text[0];
                        if ((text[0] & 0xE0) == 0xC0 && text[1]) {
                            codepoint = ((text[0] & 0x1F) << 6) | (text[1] & 0x3F);
                        }
                        ProgTP_AppHandleTextInput(&app_state, (uint32_t)codepoint);
                    }
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                ProgTP_AppAction action = ActionFromKey(event.key.key);
                ProgTP_AppHandleAction(&app_state, action);
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

        if (connectivity_job.active && atomic_load_explicit(&connectivity_job.done, memory_order_acquire)) {
            FinishConnectivityJob(&connectivity_job, &app_state);
        }
        if (!connectivity_job.active) {
            ProgTP_ConnectivityRequest connectivity_request;
            if (ProgTP_AppTakeConnectivityRequest(&app_state, &connectivity_request)) {
                StartConnectivityJob(&connectivity_job, remote_url, &app_state, &connectivity_request);
            }
        }

        if (sensor_job.active && atomic_load_explicit(&sensor_job.done, memory_order_acquire)) {
            FinishSensorJob(&sensor_job, &app_state);
        }
        if (!sensor_job.active && ProgTP_AppTakeSensorImportRequest(&app_state)) {
            StartSensorJob(&sensor_job, remote_url, &app_state);
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

    if (connectivity_job.active) {
        FinishConnectivityJob(&connectivity_job, &app_state);
    }
    if (sensor_job.active) {
        FinishSensorJob(&sensor_job, &app_state);
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
