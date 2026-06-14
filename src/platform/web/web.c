#ifndef PROGTP_WEB
#define PROGTP_WEB
#endif
#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "app.h"
#include "protocol.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define PROGTP_WEB_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define PROGTP_WEB_EXPORT
#endif

#define PROGTP_WEB_LABEL_CAPACITY 192u
#define PROGTP_WEB_JSON_CAPACITY 262144u

static char progtp_web_label[PROGTP_WEB_LABEL_CAPACITY] = "Mode: local | waiting for HTTP API";
static char progtp_web_json[PROGTP_WEB_JSON_CAPACITY];
static ProgTP_AppState progtp_web_app_state;
static Clay_RenderCommandArray progtp_web_render_commands;
static void *progtp_web_clay_memory;
static bool progtp_web_app_initialized;

static Clay_Dimensions WebMeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *user_data) {
    (void)user_data;
    float font_size = config ? (float)config->fontSize : 16.0f;
    return (Clay_Dimensions){
        .width = (float)text.length * font_size * 0.56f,
        .height = font_size * 1.25f,
    };
}

static void EnsureWebAppInitialized(void) {
    if (!progtp_web_app_initialized) {
        ProgTP_AppInit(&progtp_web_app_state, false, NULL);
        progtp_web_app_initialized = true;
    }
}

PROGTP_WEB_EXPORT void InitializeClay(float width, float height) {
    if (!progtp_web_clay_memory) {
        progtp_web_clay_memory = malloc(Clay_MinMemorySize());
    }
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(Clay_MinMemorySize(), progtp_web_clay_memory);
    Clay_Initialize(arena, (Clay_Dimensions){width, height}, (Clay_ErrorHandler){0});
    Clay_GetCurrentContext()->errorHandler = (Clay_ErrorHandler){Clay__ErrorHandlerFunctionDefault, NULL};
    Clay_SetMeasureTextFunction(WebMeasureText, NULL);
}

PROGTP_WEB_EXPORT void UpdateDrawFrame(
    uint32_t render_commands_address,
    float width,
    float height,
    float mouse_wheel_x,
    float mouse_wheel_y,
    float mouse_position_x,
    float mouse_position_y,
    bool is_touch_down,
    bool is_mouse_down,
    uint32_t action,
    uint32_t codepoint,
    float delta_time) {
    EnsureWebAppInitialized();
    bool was_input_active = progtp_web_app_state.input_mode != PROGTP_APP_INPUT_NONE || ProgTP_AppModalActive(&progtp_web_app_state);
    ProgTP_AppHandleAction(&progtp_web_app_state, (ProgTP_AppAction)action);
    if (was_input_active || action == PROGTP_APP_ACTION_NONE) {
        ProgTP_AppHandleTextInput(&progtp_web_app_state, codepoint);
    }

    Clay_SetLayoutDimensions((Clay_Dimensions){ width, height });
    Clay_SetPointerState((Clay_Vector2){ mouse_position_x, mouse_position_y }, is_touch_down || is_mouse_down);
    Clay_UpdateScrollContainers(is_touch_down || is_mouse_down, (Clay_Vector2){ mouse_wheel_x, mouse_wheel_y }, delta_time);

    progtp_web_render_commands = ProgTP_AppBuildLayout(&progtp_web_app_state, progtp_web_label, delta_time);
    memcpy((void *)(uintptr_t)render_commands_address, &progtp_web_render_commands, sizeof(progtp_web_render_commands));
}

PROGTP_WEB_EXPORT uint32_t GetRenderCommandsBuffer(void) {
    return (uint32_t)(uintptr_t)&progtp_web_render_commands;
}

PROGTP_WEB_EXPORT uint32_t GetCommandLabelBuffer(void) {
    return (uint32_t)(uintptr_t)progtp_web_label;
}

PROGTP_WEB_EXPORT uint32_t GetCommandLabelCapacity(void) {
    return PROGTP_WEB_LABEL_CAPACITY;
}

PROGTP_WEB_EXPORT uint32_t GetInventoryJsonBuffer(void) {
    return (uint32_t)(uintptr_t)progtp_web_json;
}

PROGTP_WEB_EXPORT uint32_t GetInventoryJsonCapacity(void) {
    return PROGTP_WEB_JSON_CAPACITY;
}

PROGTP_WEB_EXPORT bool ImportInventoryJson(uint32_t json_length) {
    EnsureWebAppInitialized();
    if (json_length >= PROGTP_WEB_JSON_CAPACITY) {
        ProgTP_AppSetStatus(&progtp_web_app_state, "HTTP inventory is too large");
        return false;
    }
    char error[256] = {0};
    if (!ProgTP_EquipmentInventoryFromJson(progtp_web_json, json_length, &progtp_web_app_state.inventory, error, sizeof(error))) {
        ProgTP_AppSetStatus(&progtp_web_app_state, error[0] ? error : "Invalid HTTP inventory JSON");
        return false;
    }
    ProgTP_AppUseLoadedInventory(&progtp_web_app_state, "Loaded inventory from HTTP server");
    return true;
}

PROGTP_WEB_EXPORT bool ImportSensorsJson(uint32_t json_length) {
    EnsureWebAppInitialized();
    if (json_length >= PROGTP_WEB_JSON_CAPACITY) {
        ProgTP_AppSetStatus(&progtp_web_app_state, "HTTP sensor data is too large");
        return false;
    }
    ProgTP_SensorStore store;
    ProgTP_SensorStoreInit(&store);
    char error[256] = {0};
    bool ok = ProgTP_SensorStoreFromJson(progtp_web_json, json_length, &store, error, sizeof(error));
    if (ok) {
        ProgTP_AppUseLoadedSensors(&progtp_web_app_state, &store, "Loaded sensors from HTTP server");
    } else {
        ProgTP_AppSetStatus(&progtp_web_app_state, error[0] ? error : "Invalid HTTP sensor JSON");
    }
    ProgTP_SensorStoreDestroy(&store);
    return ok;
}

PROGTP_WEB_EXPORT uint32_t ExportInventoryJson(void) {
    EnsureWebAppInitialized();
    size_t json_length = 0;
    char *json = ProgTP_EquipmentInventoryToJson(&progtp_web_app_state.inventory, &json_length);
    if (!json) {
        ProgTP_AppSetStatus(&progtp_web_app_state, "Could not serialize inventory");
        return 0;
    }
    if (json_length + 1u > PROGTP_WEB_JSON_CAPACITY) {
        free(json);
        ProgTP_AppSetStatus(&progtp_web_app_state, "Inventory JSON buffer is too small");
        return 0;
    }
    memcpy(progtp_web_json, json, json_length);
    progtp_web_json[json_length] = '\0';
    free(json);
    return (uint32_t)json_length;
}

PROGTP_WEB_EXPORT bool IsInventoryDirty(void) {
    EnsureWebAppInitialized();
    return ProgTP_AppInventoryDirty(&progtp_web_app_state);
}

PROGTP_WEB_EXPORT uint32_t GetInventoryVersion(void) {
    EnsureWebAppInitialized();
    return (uint32_t)ProgTP_AppInventoryVersion(&progtp_web_app_state);
}

PROGTP_WEB_EXPORT void MarkInventoryClean(void) {
    EnsureWebAppInitialized();
    ProgTP_AppMarkInventoryClean(&progtp_web_app_state);
    ProgTP_AppSetStatus(&progtp_web_app_state, "Saved inventory to HTTP server");
}

PROGTP_WEB_EXPORT uint32_t ExportConnectivityRequestJson(void) {
    EnsureWebAppInitialized();
    ProgTP_ConnectivityRequest request;
    if (!ProgTP_AppTakeConnectivityRequest(&progtp_web_app_state, &request)) {
        return 0;
    }
    size_t json_length = 0;
    char *json = ProgTP_ConnectivityRequestToJson(&request, &json_length);
    if (!json || json_length + 1u > PROGTP_WEB_JSON_CAPACITY) {
        free(json);
        ProgTP_AppFailConnectivityRequest(&progtp_web_app_state, "Could not serialize connectivity request");
        return 0;
    }
    memcpy(progtp_web_json, json, json_length);
    progtp_web_json[json_length] = '\0';
    free(json);
    return (uint32_t)json_length;
}

PROGTP_WEB_EXPORT bool ImportConnectivityResultJson(uint32_t json_length) {
    EnsureWebAppInitialized();
    if (json_length >= PROGTP_WEB_JSON_CAPACITY) {
        ProgTP_AppFailConnectivityRequest(&progtp_web_app_state, "Connectivity result is too large");
        return false;
    }
    ProgTP_ConnectivityResult result;
    char error[256] = {0};
    if (!ProgTP_ConnectivityResultFromJson(
            progtp_web_json,
            json_length,
            &result,
            error,
            sizeof(error))) {
        ProgTP_AppFailConnectivityRequest(
            &progtp_web_app_state,
            error[0] ? error : "Invalid connectivity result");
        return false;
    }
    ProgTP_AppCompleteConnectivityRequest(&progtp_web_app_state, &result, false);
    return true;
}

PROGTP_WEB_EXPORT void FailConnectivityRequest(uint32_t message_length) {
    EnsureWebAppInitialized();
    if (message_length >= PROGTP_WEB_JSON_CAPACITY) {
        message_length = PROGTP_WEB_JSON_CAPACITY - 1u;
    }
    progtp_web_json[message_length] = '\0';
    ProgTP_AppFailConnectivityRequest(&progtp_web_app_state, progtp_web_json);
}

PROGTP_WEB_EXPORT bool TakeSensorImportRequest(void) {
    EnsureWebAppInitialized();
    return ProgTP_AppTakeSensorImportRequest(&progtp_web_app_state);
}

PROGTP_WEB_EXPORT bool ImportSensorImportResultJson(uint32_t json_length) {
    EnsureWebAppInitialized();
    if (json_length >= PROGTP_WEB_JSON_CAPACITY) {
        ProgTP_AppFailSensorImport(&progtp_web_app_state, "Sensor import response is too large");
        return false;
    }
    ProgTP_SensorStore store;
    ProgTP_SensorStoreInit(&store);
    ProgTP_SensorImportResult result;
    char error[256] = {0};
    bool ok = ProgTP_SensorImportResponseFromJson(
        progtp_web_json,
        json_length,
        &result,
        &store,
        error,
        sizeof(error));
    if (ok) {
        ProgTP_AppCompleteSensorImport(&progtp_web_app_state, &result, &store);
    } else {
        ProgTP_AppFailSensorImport(
            &progtp_web_app_state,
            error[0] ? error : "Invalid sensor import result");
    }
    ProgTP_SensorStoreDestroy(&store);
    return ok;
}

PROGTP_WEB_EXPORT void FailSensorImport(uint32_t message_length) {
    EnsureWebAppInitialized();
    if (message_length >= PROGTP_WEB_JSON_CAPACITY) {
        message_length = PROGTP_WEB_JSON_CAPACITY - 1u;
    }
    progtp_web_json[message_length] = '\0';
    ProgTP_AppFailSensorImport(&progtp_web_app_state, progtp_web_json);
}

PROGTP_WEB_EXPORT bool ImportIncidentsJson(uint32_t json_length) {
    EnsureWebAppInitialized();
    if (json_length >= PROGTP_WEB_JSON_CAPACITY) {
        ProgTP_AppSetStatus(&progtp_web_app_state, "HTTP incident data is too large");
        return false;
    }
    ProgTP_IncidentStore store;
    ProgTP_IncidentStoreInit(&store);
    char error[256] = {0};
    bool ok = ProgTP_IncidentStoreFromJson(progtp_web_json, json_length, &store, error, sizeof(error));
    if (ok) {
        ProgTP_AppUseLoadedIncidents(&progtp_web_app_state, &store, "Loaded incidents from HTTP server");
    } else {
        ProgTP_AppSetStatus(&progtp_web_app_state, error[0] ? error : "Invalid HTTP incident JSON");
    }
    ProgTP_IncidentStoreDestroy(&store);
    return ok;
}

PROGTP_WEB_EXPORT uint32_t ExportIncidentsJson(void) {
    EnsureWebAppInitialized();
    size_t json_length = 0;
    char *json = ProgTP_IncidentStoreToJson(&progtp_web_app_state.incidents, &json_length);
    if (!json) {
        ProgTP_AppSetStatus(&progtp_web_app_state, "Could not serialize incidents");
        return 0;
    }
    if (json_length + 1u > PROGTP_WEB_JSON_CAPACITY) {
        free(json);
        ProgTP_AppSetStatus(&progtp_web_app_state, "Incident JSON buffer is too small");
        return 0;
    }
    memcpy(progtp_web_json, json, json_length);
    progtp_web_json[json_length] = '\0';
    free(json);
    return (uint32_t)json_length;
}

PROGTP_WEB_EXPORT bool TakeIncidentOperationRequest(void) {
    EnsureWebAppInitialized();
    ProgTP_IncidentOperationRequest request;
    return ProgTP_AppTakeIncidentOperationRequest(&progtp_web_app_state, &request);
}

PROGTP_WEB_EXPORT uint32_t ExportIncidentOperationRequestJson(void) {
    EnsureWebAppInitialized();
    ProgTP_IncidentOperationRequest request;
    if (!ProgTP_AppTakeIncidentOperationRequest(&progtp_web_app_state, &request)) {
        return 0;
    }
    size_t json_length = 0;
    char *json = ProgTP_IncidentOperationRequestToJson(&request, &json_length);
    if (!json || json_length + 1u > PROGTP_WEB_JSON_CAPACITY) {
        free(json);
        ProgTP_AppFailIncidentOperation(&progtp_web_app_state, "Could not serialize incident operation request");
        return 0;
    }
    memcpy(progtp_web_json, json, json_length);
    progtp_web_json[json_length] = '\0';
    free(json);
    return (uint32_t)json_length;
}

PROGTP_WEB_EXPORT bool ImportIncidentOperationResponseJson(uint32_t json_length) {
    EnsureWebAppInitialized();
    if (json_length >= PROGTP_WEB_JSON_CAPACITY) {
        ProgTP_AppFailIncidentOperation(&progtp_web_app_state, "Incident operation response is too large");
        return false;
    }
    ProgTP_IncidentOperationResponse response;
    char error[256] = {0};
    if (!ProgTP_IncidentOperationResponseFromJson(progtp_web_json, json_length, &response, error, sizeof(error))) {
        ProgTP_AppFailIncidentOperation(
            &progtp_web_app_state,
            error[0] ? error : "Invalid incident operation response");
        return false;
    }
    ProgTP_AppCompleteIncidentOperation(&progtp_web_app_state, &response);
    return true;
}

PROGTP_WEB_EXPORT void FailIncidentOperation(uint32_t message_length) {
    EnsureWebAppInitialized();
    if (message_length >= PROGTP_WEB_JSON_CAPACITY) {
        message_length = PROGTP_WEB_JSON_CAPACITY - 1u;
    }
    progtp_web_json[message_length] = '\0';
    ProgTP_AppFailIncidentOperation(&progtp_web_app_state, progtp_web_json);
}
