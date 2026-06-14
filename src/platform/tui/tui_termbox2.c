#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "../../../subprojects/clay/renderers/termbox2/clay_renderer_termbox2.c"

#include "app.h"
#include "command_client.h"

#define TB_IMPL
#include <termbox2.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <threads.h>

typedef struct {
    thrd_t thread;
    atomic_bool done;
    bool active;
    bool has_thread;
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
    thrd_t thread;
    atomic_bool done;
    bool active;
    bool has_thread;
    bool remote;
    bool ok;
    char remote_url[512];
    char sensor_input_path[512];
    char error[256];
    ProgTP_SensorStore store;
    ProgTP_SensorImportResult result;
} SensorJob;

static ProgTP_AppAction ActionFromTermboxEvent(const struct tb_event *event) {
    if (event->key == TB_KEY_BACKSPACE || event->key == TB_KEY_BACKSPACE2) {
        return PROGTP_APP_ACTION_INPUT_BACKSPACE;
    }
    if (event->key == TB_KEY_ENTER) {
        return PROGTP_APP_ACTION_INPUT_SUBMIT;
    }
    if (event->key == TB_KEY_TAB) {
        return PROGTP_APP_ACTION_FORM_NEXT_FIELD;
    }
    switch (event->ch) {
        case 'n': case 'N': return PROGTP_APP_ACTION_NEXT;
        case 'p': case 'P': return PROGTP_APP_ACTION_PREVIOUS;
        case 'a': case 'A': return PROGTP_APP_ACTION_ADD_SAMPLE;
        case 'u': case 'U': return PROGTP_APP_ACTION_UPDATE_SELECTED;
        case 'r': case 'R': return PROGTP_APP_ACTION_REMOVE_SELECTED;
        case '1': return PROGTP_APP_ACTION_MODULE_1;
        case '2': return PROGTP_APP_ACTION_MODULE_2;
        case '3': return PROGTP_APP_ACTION_MODULE_3;
        case '4': return PROGTP_APP_ACTION_MODULE_4;
        case '5': return PROGTP_APP_ACTION_MODULE_5;
        case '6': return PROGTP_APP_ACTION_MODULE_6;
        case 'c': case 'C': return PROGTP_APP_ACTION_SEARCH_CODE;
        case 'g': case 'G': return PROGTP_APP_ACTION_SENSOR_IMPORT;
        case 'o': case 'O': return PROGTP_APP_ACTION_SENSOR_CHOOSE_FILE;
        case 'i': case 'I': return PROGTP_APP_ACTION_SEARCH_IP;
        case 'm': case 'M': return PROGTP_APP_ACTION_SEARCH_MAC;
        case 'd': case 'D': return PROGTP_APP_ACTION_INCIDENT_DELETE;
        case 'e': case 'E': return PROGTP_APP_ACTION_INCIDENT_EDIT;
        case 's': case 'S': return PROGTP_APP_ACTION_INCIDENT_START;
        case 't': case 'T': return PROGTP_APP_ACTION_INCIDENT_AUTO_IMPORT;
        case 'q': case 'Q': return PROGTP_APP_ACTION_INCIDENT_ADD;
        case 'z': case 'Z': return PROGTP_APP_ACTION_CONFIG_UNDO;
        case 'y': case 'Y': return PROGTP_APP_ACTION_CONFIG_REDO;
        case 'f': case 'F': return PROGTP_APP_ACTION_CONFIG_FILTER_ALL;
        case 'b': case 'B': return PROGTP_APP_ACTION_CONFIG_IMPORT;
        case 'x': case 'X': return PROGTP_APP_ACTION_CONFIG_DELETE;
        case 'w': case 'W': return PROGTP_APP_ACTION_SAVE;
        case 'l': case 'L': return PROGTP_APP_ACTION_LOAD;
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
    size_t count = ProgTP_EquipmentInventoryGetCount(source);
    ProgTP_Equipment *items = NULL;
    if (count > 0) {
        items = calloc(count, sizeof(*items));
        if (!items) {
            snprintf(error, error_size, "not enough memory to copy inventory");
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(source, i);
            if (equipment) {
                items[i] = *equipment;
            }
        }
    }
    bool ok = ProgTP_EquipmentInventoryReplace(destination, items, count, source->next_code, error, error_size);
    free(items);
    return ok;
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
    if (thrd_create(&job->thread, ConnectivityThreadMain, job) != thrd_success) {
        snprintf(job->error, sizeof(job->error), "could not start connectivity worker");
        job->ok = false;
        atomic_store(&job->done, true);
        return;
    }
    job->has_thread = true;
}

static void FinishConnectivityJob(ConnectivityJob *job, ProgTP_AppState *app_state) {
    if (job->has_thread) {
        int ignored = 0;
        thrd_join(job->thread, &ignored);
        job->has_thread = false;
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
    if (thrd_create(&job->thread, SensorThreadMain, job) != thrd_success) {
        snprintf(job->error, sizeof(job->error), "could not start sensor worker");
        job->ok = false;
        atomic_store(&job->done, true);
        return;
    }
    job->has_thread = true;
}

static void FinishSensorJob(SensorJob *job, ProgTP_AppState *app_state) {
    if (job->has_thread) {
        int ignored = 0;
        thrd_join(job->thread, &ignored);
        job->has_thread = false;
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
        ProgTP_RunLocalCommand(&command_result);
    }
    char command_label[192];
    ProgTP_FormatCommandResultLabel(&command_result, command_label, sizeof(command_label));
    ProgTP_AppState app_state;
    ProgTP_AppInit(&app_state, remote_url == NULL, "equipamentos.dat");
    ProgTP_AppSetTerminalRendering(&app_state, true);
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

    Clay_Termbox_Initialize(
        TB_OUTPUT_256,
        CLAY_TB_BORDER_MODE_DEFAULT,
        CLAY_TB_BORDER_CHARS_DEFAULT,
        CLAY_TB_IMAGE_MODE_PLACEHOLDER,
        false);
    tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);

    uint64_t clay_memory_size = Clay_MinMemorySize();
    Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, malloc(clay_memory_size));
    Clay_Initialize(clay_arena, (Clay_Dimensions){ Clay_Termbox_Width(), Clay_Termbox_Height() }, (Clay_ErrorHandler){ ProgTP_HandleClayError, NULL });
    Clay_SetMeasureTextFunction(Clay_Termbox_MeasureText, NULL);

    Clay_RenderCommandArray commands = ProgTP_AppBuildLayout(&app_state, command_label, 0.0f);
    tb_clear();
    Clay_Termbox_Render(commands);
    tb_present();

    bool running = true;
    uint64_t last_save_attempt_version = 0;
    int mouse_x = 0;
    int mouse_y = 0;
    bool mouse_down = false;
    ConnectivityJob connectivity_job = {0};
    SensorJob sensor_job = {0};
    while (running) {
        struct tb_event event;
        int event_result = tb_peek_event(&event, 16);
        if (event_result == TB_OK) {
            bool text_active = app_state.input_mode != PROGTP_APP_INPUT_NONE || ProgTP_AppModalActive(&app_state);
            if (event.type == TB_EVENT_KEY && (event.key == TB_KEY_CTRL_C || (!text_active && (event.ch == 'q' || event.ch == 'Q')))) {
                running = false;
            } else if (event.type == TB_EVENT_KEY && event.key == TB_KEY_ESC) {
                if (ProgTP_AppModalActive(&app_state)) {
                    ProgTP_AppHandleAction(&app_state, PROGTP_APP_ACTION_FORM_CANCEL);
                } else if (text_active) {
                    ProgTP_AppSetStatus(&app_state, "Canceled");
                } else {
                    running = false;
                }
            } else if (event.type == TB_EVENT_RESIZE) {
                Clay_SetLayoutDimensions((Clay_Dimensions){ Clay_Termbox_Width(), Clay_Termbox_Height() });
            } else if (event.type == TB_EVENT_KEY) {
                ProgTP_AppAction action = ActionFromTermboxEvent(&event);
                if ((app_state.input_mode != PROGTP_APP_INPUT_NONE || ProgTP_AppModalActive(&app_state)) &&
                    action != PROGTP_APP_ACTION_INPUT_BACKSPACE &&
                    action != PROGTP_APP_ACTION_INPUT_SUBMIT &&
                    event.ch >= 32 &&
                    event.ch <= 126) {
                    ProgTP_AppHandleTextInput(&app_state, event.ch);
                } else if (action != PROGTP_APP_ACTION_NONE) {
                    ProgTP_AppHandleAction(&app_state, action);
                }
            } else if (event.type == TB_EVENT_MOUSE) {
                mouse_x = (int)(((float)event.x + 0.5f) * Clay_Termbox_Cell_Width());
                mouse_y = (int)(((float)event.y + 0.5f) * Clay_Termbox_Cell_Height());
                if (event.key == TB_KEY_MOUSE_LEFT) {
                    mouse_down = true;
                } else if (event.key == TB_KEY_MOUSE_RELEASE) {
                    mouse_down = false;
                } else if (event.key == TB_KEY_MOUSE_WHEEL_UP) {
                    Clay_UpdateScrollContainers(mouse_down, (Clay_Vector2){0.0f, 1.0f}, 0.016f);
                } else if (event.key == TB_KEY_MOUSE_WHEEL_DOWN) {
                    Clay_UpdateScrollContainers(mouse_down, (Clay_Vector2){0.0f, -1.0f}, 0.016f);
                }
            }
        }

        Clay_SetLayoutDimensions((Clay_Dimensions){ Clay_Termbox_Width(), Clay_Termbox_Height() });
        Clay_SetPointerState((Clay_Vector2){ (float)mouse_x, (float)mouse_y }, mouse_down);

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

        commands = ProgTP_AppBuildLayout(&app_state, command_label, 0.016f);

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

        tb_clear();
        Clay_Termbox_Render(commands);
        tb_present();
    }

    if (connectivity_job.active) {
        FinishConnectivityJob(&connectivity_job, &app_state);
    }
    if (sensor_job.active) {
        FinishSensorJob(&sensor_job, &app_state);
    }

    Clay_Termbox_Close();
    free(clay_arena.memory);
    ProgTP_AppDestroy(&app_state);
    return 0;
}
