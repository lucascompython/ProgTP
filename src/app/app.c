#include "app.h"
#include "app_internal.h"

#include "progtp_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t CStringLength(const char *value) {
    int32_t length = 0;
    while (value[length] != '\0') {
        ++length;
    }
    return length;
}

Clay_String StringFromCString(const char *value) {
    return (Clay_String){
        .length = CStringLength(value),
        .chars = (char *)value,
        .isStaticallyAllocated = false,
    };
}

const Clay_Color COLOR_PAGE = {25, 27, 30, 255};
const Clay_Color COLOR_SIDEBAR = {35, 39, 45, 255};
const Clay_Color COLOR_SURFACE = {246, 248, 250, 255};
const Clay_Color COLOR_SURFACE_ALT = {235, 240, 245, 255};
const Clay_Color COLOR_SURFACE_DARK = {223, 230, 237, 255};
const Clay_Color COLOR_ACCENT = {36, 121, 108, 255};
const Clay_Color COLOR_ACCENT_DARK = {24, 88, 80, 255};
const Clay_Color COLOR_DANGER = {176, 54, 63, 255};
const Clay_Color COLOR_TEXT = {22, 26, 31, 255};
const Clay_Color COLOR_MUTED = {91, 99, 110, 255};
const Clay_Color COLOR_WHITE = {255, 255, 255, 255};
const Clay_Color COLOR_LINE = {206, 214, 222, 255};
const Clay_Color COLOR_OVERLAY = {9, 12, 16, 178};

ProgTP_AppState *progtp_interaction_state;
bool progtp_ui_compact;

float ControlHeight(void) {
    return progtp_ui_compact ? 42.0f : 34.0f;
}

float ButtonMinWidth(void) {
    return progtp_ui_compact ? 52.0f : 86.0f;
}

uint16_t ControlGap(void) {
    return progtp_ui_compact ? 4u : 8u;
}

void ProgTP_HandleClayError(Clay_ErrorData errorData) {
#if !defined(CLAY_WASM) && !defined(PROGTP_WEB)
    fprintf(stderr, "Clay error: %.*s\n", (int)errorData.errorText.length, errorData.errorText.chars);
#else
    (void)errorData;
#endif
}

const char *ModuleName(int module) {
    static const char *names[] = {
        "Equipment Inventory",
        "Connectivity Tests",
        "Sensor Monitoring",
        "Incident Queue",
        "Configuration Stack",
        "Files and Reports",
        "Technicians",
        "Settings",
    };
    if (module < 1 || module > 8) {
        return "Module";
    }
    return names[module - 1];
}

const char *InputModeName(ProgTP_AppInputMode mode) {
    switch (mode) {
        case PROGTP_APP_INPUT_SEARCH_CODE: return "Search code";
        case PROGTP_APP_INPUT_SEARCH_IP: return "Search IP";
        case PROGTP_APP_INPUT_SEARCH_MAC: return "Search MAC";
        case PROGTP_APP_INPUT_CONNECTIVITY_COMMAND: return "Custom command";
        case PROGTP_APP_INPUT_SENSOR_CODE: return "Sensor code";
        case PROGTP_APP_INPUT_NONE: break;
    }
    return "";
}

const char *SearchPlaceholder(ProgTP_AppInputMode mode) {
    switch (mode) {
        case PROGTP_APP_INPUT_SEARCH_CODE: return "Enter equipment code";
        case PROGTP_APP_INPUT_SEARCH_IP: return "Enter IP address";
        case PROGTP_APP_INPUT_SEARCH_MAC: return "Enter MAC address";
        case PROGTP_APP_INPUT_CONNECTIVITY_COMMAND: return "Enter a command to run";
        case PROGTP_APP_INPUT_SENSOR_CODE: return "Enter sensor code";
        case PROGTP_APP_INPUT_NONE: break;
    }
    return "Select Code, IP, or MAC";
}

ProgTP_Equipment *SelectedEquipment(ProgTP_AppState *state) {
    return ProgTP_EquipmentInventoryFindByCode(&state->inventory, state->selected_code);
}

void EnsureSelection(ProgTP_AppState *state) {
    if (state->inventory.array.length == 0) {
        state->selected_code = 0;
        return;
    }
    if (!SelectedEquipment(state)) {
        state->selected_code = state->inventory.array.items[0].code;
    }
}

void ProgTP_AppSetStatus(ProgTP_AppState *state, const char *message) {
    snprintf(state->status, sizeof(state->status), "%s", message ? message : "");
}

void ProgTP_AppSetTerminalRendering(ProgTP_AppState *state, bool enabled) {
    state->terminal_rendering = enabled;
}

void SetStatus(ProgTP_AppState *state, const char *message) {
    ProgTP_AppSetStatus(state, message);
}

static void PersistIfEnabled(ProgTP_AppState *state) {
    if (!state->persistence_enabled) {
        return;
    }
    char error[256] = {0};
    if (!ProgTP_EquipmentInventorySaveBinary(&state->inventory, state->storage_path, error, sizeof(error))) {
        snprintf(state->status, sizeof(state->status), "Save failed: %s", error);
    } else {
        state->inventory_dirty = false;
    }
}

void MarkInventoryChanged(ProgTP_AppState *state) {
    state->inventory_dirty = true;
    ++state->inventory_version;
    PersistIfEnabled(state);
}

static void LoadInventory(ProgTP_AppState *state) {
    char error[256] = {0};
    if (state->persistence_enabled && ProgTP_EquipmentInventoryLoadBinary(&state->inventory, state->storage_path, error, sizeof(error))) {
        SetStatus(state, "Loaded equipamentos.dat");
    } else {
        ProgTP_EquipmentInventorySeedDefaults(&state->inventory);
        SetStatus(state, state->persistence_enabled ? "Created default inventory" : "Using in-memory web inventory");
        PersistIfEnabled(state);
    }
    EnsureSelection(state);
}

static void LoadSensors(ProgTP_AppState *state) {
    if (!state->persistence_enabled) {
        return;
    }
    char error[256] = {0};
    if (!ProgTP_SensorStoreLoadBinary(&state->sensors, "leituras_sensores.dat", error, sizeof(error))) {
        snprintf(state->status, sizeof(state->status), "Sensor load failed: %s", error);
    }
    EnsureSensorSelection(state);
}

static void PersistSensorsIfEnabled(ProgTP_AppState *state) {
    if (!state->persistence_enabled) {
        return;
    }
    char error[256] = {0};
    if (!ProgTP_SensorStoreSaveBinary(&state->sensors, "leituras_sensores.dat", error, sizeof(error))) {
        snprintf(state->status, sizeof(state->status), "Sensor save failed: %s", error);
    }
}

void ProgTP_AppInit(ProgTP_AppState *state, bool persistence_enabled, const char *storage_path) {
    memset(state, 0, sizeof(*state));
    state->active_module = 1;
    state->persistence_enabled = persistence_enabled;
    snprintf(state->sensor_input_path, sizeof(state->sensor_input_path), "%s", "sensores_rack.txt");
    snprintf(
        state->connectivity_result.summary,
        sizeof(state->connectivity_result.summary),
        "%s",
        "No connectivity test has been run");
    snprintf(state->storage_path, sizeof(state->storage_path), "%s", storage_path ? storage_path : "equipamentos.dat");
    ProgTP_EquipmentInventoryInit(&state->inventory);
    ProgTP_SensorStoreInit(&state->sensors);
    LoadInventory(state);
    LoadSensors(state);
}

void ProgTP_AppDestroy(ProgTP_AppState *state) {
    PersistIfEnabled(state);
    PersistSensorsIfEnabled(state);
    ProgTP_SensorStoreDestroy(&state->sensors);
    ProgTP_EquipmentInventoryDestroy(&state->inventory);
}

void ProgTP_AppUseLoadedInventory(ProgTP_AppState *state, const char *status) {
    state->modal = PROGTP_APP_MODAL_NONE;
    state->input_mode = PROGTP_APP_INPUT_NONE;
    state->input_text[0] = '\0';
    state->row_offset = 0;
    state->inventory_dirty = false;
    ++state->inventory_version;
    EnsureSelection(state);
    SetStatus(state, status ? status : "Loaded inventory");
}

bool ProgTP_AppInventoryDirty(const ProgTP_AppState *state) {
    return state->inventory_dirty;
}

uint64_t ProgTP_AppInventoryVersion(const ProgTP_AppState *state) {
    return state->inventory_version;
}

void ProgTP_AppMarkInventoryClean(ProgTP_AppState *state) {
    state->inventory_dirty = false;
}

bool ProgTP_AppModalActive(const ProgTP_AppState *state) {
    return state->modal != PROGTP_APP_MODAL_NONE;
}

void ProgTP_AppUseLoadedSensors(ProgTP_AppState *state, const ProgTP_SensorStore *store, const char *status) {
    char error[256] = {0};
    if (!ProgTP_SensorStoreCopy(&state->sensors, store, error, sizeof(error))) {
        snprintf(state->status, sizeof(state->status), "Sensor load failed: %s", error);
        return;
    }
    state->sensor_import_request_pending = false;
    state->sensor_import_request_in_flight = false;
    state->sensor_row_offset = 0;
    EnsureSensorSelection(state);
    SetStatus(state, status ? status : "Loaded sensors");
}

bool ProgTP_AppTakeConnectivityRequest(ProgTP_AppState *state, ProgTP_ConnectivityRequest *request) {
    if (!state->connectivity_request_pending || state->connectivity_request_in_flight) {
        return false;
    }
    *request = state->connectivity_request;
    state->connectivity_request_pending = false;
    state->connectivity_request_in_flight = true;
    SetStatus(state, "Running connectivity command");
    return true;
}

bool ProgTP_AppTakeSensorImportRequest(ProgTP_AppState *state) {
    if (!state->sensor_import_request_pending || state->sensor_import_request_in_flight) {
        return false;
    }
    state->sensor_import_request_pending = false;
    state->sensor_import_request_in_flight = true;
    SetStatus(state, "Importing sensor readings");
    return true;
}

void ProgTP_AppCompleteConnectivityRequest(
    ProgTP_AppState *state,
    const ProgTP_ConnectivityResult *result,
    bool inventory_changed_locally) {
    state->connectivity_result = *result;
    state->connectivity_has_result = true;
    state->connectivity_request_in_flight = false;
    if (inventory_changed_locally && result->inventory_changed) {
        MarkInventoryChanged(state);
    }
    SetStatus(state, result->summary);
    EnsureSelection(state);
}

void ProgTP_AppFailConnectivityRequest(ProgTP_AppState *state, const char *error) {
    state->connectivity_request_pending = false;
    state->connectivity_request_in_flight = false;
    snprintf(
        state->connectivity_result.summary,
        sizeof(state->connectivity_result.summary),
        "Connectivity command failed: %s",
        error ? error : "unknown error");
    state->connectivity_has_result = true;
    SetStatus(state, state->connectivity_result.summary);
}

void ProgTP_AppCompleteSensorImport(
    ProgTP_AppState *state,
    const ProgTP_SensorImportResult *result,
    const ProgTP_SensorStore *store) {
    char copy_error[256] = {0};
    if (!ProgTP_SensorStoreCopy(&state->sensors, store, copy_error, sizeof(copy_error))) {
        ProgTP_AppFailSensorImport(state, copy_error);
        return;
    }
    state->sensor_import_result = *result;
    state->sensor_has_import_result = true;
    state->sensor_import_request_in_flight = false;
    state->selected_sensor_index = state->sensors.length > 0 ? state->sensors.length - 1u : 0;
    state->sensor_row_offset = 0;
    SetStatus(state, result->summary);
}

void ProgTP_AppFailSensorImport(ProgTP_AppState *state, const char *error) {
    state->sensor_import_request_pending = false;
    state->sensor_import_request_in_flight = false;
    snprintf(
        state->status,
        sizeof(state->status),
        "Sensor import failed: %s",
        error ? error : "unknown error");
}

static void StartInput(ProgTP_AppState *state, ProgTP_AppInputMode mode) {
    state->input_mode = mode;
    if (mode != PROGTP_APP_INPUT_CONNECTIVITY_COMMAND) {
        state->input_text[0] = '\0';
    }
    snprintf(state->status, sizeof(state->status), "%s: type value and press Enter", InputModeName(mode));
}

static void FocusSearchInput(ProgTP_AppState *state) {
    if (state->input_mode == PROGTP_APP_INPUT_NONE) {
        StartInput(state, PROGTP_APP_INPUT_SEARCH_CODE);
    } else {
        snprintf(state->status, sizeof(state->status), "%s: %s", InputModeName(state->input_mode), state->input_text);
    }
}

static void SubmitInput(ProgTP_AppState *state) {
    if (state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND) {
        QueueConnectivityRequest(state, PROGTP_CONNECTIVITY_CUSTOM);
        return;
    }
    if (state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE) {
        const ProgTP_SensorReading *reading = ProgTP_SensorStoreFindLatestByCode(&state->sensors, state->sensor_search_text);
        if (reading) {
            state->selected_sensor_index = (size_t)(reading - state->sensors.items);
            snprintf(state->status, sizeof(state->status), "Found sensor %s", reading->code);
        } else {
            snprintf(state->status, sizeof(state->status), "No sensor match for %s", state->sensor_search_text);
        }
        state->input_mode = PROGTP_APP_INPUT_NONE;
        state->sensor_search_text[0] = '\0';
        return;
    }
    ProgTP_Equipment *equipment = NULL;
    if (state->input_mode == PROGTP_APP_INPUT_SEARCH_CODE) {
        equipment = ProgTP_EquipmentInventoryFindByCode(&state->inventory, (uint32_t)strtoul(state->input_text, NULL, 10));
    } else if (state->input_mode == PROGTP_APP_INPUT_SEARCH_IP) {
        equipment = ProgTP_EquipmentInventoryFindByIp(&state->inventory, state->input_text);
    } else if (state->input_mode == PROGTP_APP_INPUT_SEARCH_MAC) {
        equipment = ProgTP_EquipmentInventoryFindByMac(&state->inventory, state->input_text);
    }
    if (equipment) {
        state->selected_code = equipment->code;
        snprintf(state->status, sizeof(state->status), "Found equipment #%u", equipment->code);
    } else {
        snprintf(state->status, sizeof(state->status), "No match for %s", state->input_text);
    }
    state->input_mode = PROGTP_APP_INPUT_NONE;
    state->input_text[0] = '\0';
}

static void ResetRowNavigation(ProgTP_AppState *state) {
    state->row_offset = 0;
    EnsureSelectionInCurrentView(state);
}

static bool IsInventoryMutationAction(ProgTP_AppAction action) {
    switch (action) {
        case PROGTP_APP_ACTION_ADD_SAMPLE:
        case PROGTP_APP_ACTION_UPDATE_SELECTED:
        case PROGTP_APP_ACTION_REMOVE_SELECTED:
        case PROGTP_APP_ACTION_CYCLE_STATE:
        case PROGTP_APP_ACTION_TOGGLE_PENDING:
        case PROGTP_APP_ACTION_SAVE:
        case PROGTP_APP_ACTION_LOAD:
            return true;
        default:
            return false;
    }
}

void ProgTP_AppHandleAction(ProgTP_AppState *state, ProgTP_AppAction action) {
    if (HandleModalAction(state, action)) {
        EnsureSelection(state);
        return;
    }

    if (state->input_mode != PROGTP_APP_INPUT_NONE) {
        if (action == PROGTP_APP_ACTION_INPUT_BACKSPACE) {
            char *buffer = state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND
                ? state->connectivity_custom_command
                : state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE ? state->sensor_search_text : state->input_text;
            size_t length = strlen(buffer);
            if (length > 0) {
                buffer[length - 1u] = '\0';
            }
            return;
        }
        if (action == PROGTP_APP_ACTION_INPUT_SUBMIT) {
            SubmitInput(state);
            return;
        }
        if (state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND &&
            action == PROGTP_APP_ACTION_CONNECTIVITY_RUN_CUSTOM) {
            QueueConnectivityRequest(state, PROGTP_CONNECTIVITY_CUSTOM);
            return;
        }
        if (action != PROGTP_APP_ACTION_NONE) {
            return;
        }
    }

    if (state->connectivity_request_in_flight && IsInventoryMutationAction(action)) {
        SetStatus(state, "Wait for the connectivity command to finish");
        return;
    }

    switch (action) {
        case PROGTP_APP_ACTION_NEXT:
            if (state->active_module == 3) {
                MoveSensorSelection(state, 1);
            } else {
                MoveSelection(state, 1);
            }
            break;
        case PROGTP_APP_ACTION_PREVIOUS:
            if (state->active_module == 3) {
                MoveSensorSelection(state, -1);
            } else {
                MoveSelection(state, -1);
            }
            break;
        case PROGTP_APP_ACTION_ADD_SAMPLE: OpenAddModal(state); break;
        case PROGTP_APP_ACTION_UPDATE_SELECTED: OpenUpdateModal(state); break;
        case PROGTP_APP_ACTION_REMOVE_SELECTED: OpenRemoveModal(state); break;
        case PROGTP_APP_ACTION_CYCLE_STATE:
        case PROGTP_APP_ACTION_TOGGLE_PENDING:
            OpenUpdateModal(state);
            break;
        case PROGTP_APP_ACTION_FILTER_ALL:
            state->state_filter_enabled = false;
            state->type_filter[0] = '\0';
            ResetRowNavigation(state);
            break;
        case PROGTP_APP_ACTION_FILTER_ROUTERS: SetTypeFilter(state, "Router"); break;
        case PROGTP_APP_ACTION_FILTER_FAILED: SetStateFilter(state, true, PROGTP_EQUIPMENT_FAILED); break;
        case PROGTP_APP_ACTION_FILTER_PENDING: break;
        case PROGTP_APP_ACTION_SEARCH_CODE:
            if (state->active_module == 3) {
                state->input_mode = PROGTP_APP_INPUT_SENSOR_CODE;
                state->sensor_search_text[0] = '\0';
                SetStatus(state, "Sensor code: type value and press Enter");
            } else {
                StartInput(state, PROGTP_APP_INPUT_SEARCH_CODE);
            }
            break;
        case PROGTP_APP_ACTION_SEARCH_IP: StartInput(state, PROGTP_APP_INPUT_SEARCH_IP); break;
        case PROGTP_APP_ACTION_SEARCH_MAC: StartInput(state, PROGTP_APP_INPUT_SEARCH_MAC); break;
        case PROGTP_APP_ACTION_SEARCH_FIELD: FocusSearchInput(state); break;
        case PROGTP_APP_ACTION_SAVE:
            if (state->persistence_enabled) {
                PersistIfEnabled(state);
                SetStatus(state, "Saved inventory");
            } else {
                SetStatus(state, "Inventory saves through the active connection");
            }
            break;
        case PROGTP_APP_ACTION_LOAD:
            if (state->persistence_enabled) {
                ProgTP_EquipmentInventoryClear(&state->inventory);
                LoadInventory(state);
            } else {
                SetStatus(state, "Reload is handled by the active connection");
            }
            break;
        case PROGTP_APP_ACTION_MODULE_1: state->active_module = 1; break;
        case PROGTP_APP_ACTION_MODULE_2: state->active_module = 2; break;
        case PROGTP_APP_ACTION_MODULE_3: state->active_module = 3; break;
        case PROGTP_APP_ACTION_MODULE_4: state->active_module = 4; break;
        case PROGTP_APP_ACTION_MODULE_5: state->active_module = 5; break;
        case PROGTP_APP_ACTION_MODULE_6: state->active_module = 6; break;
        case PROGTP_APP_ACTION_MODULE_7: state->active_module = 7; break;
        case PROGTP_APP_ACTION_MODULE_8: state->active_module = 8; break;
        case PROGTP_APP_ACTION_FORM_SUBMIT:
        case PROGTP_APP_ACTION_FORM_CANCEL:
        case PROGTP_APP_ACTION_FORM_NEXT_FIELD:
        case PROGTP_APP_ACTION_FORM_PREVIOUS_FIELD:
        case PROGTP_APP_ACTION_FORM_TOGGLE_PENDING:
        case PROGTP_APP_ACTION_FORM_STATE_PREVIOUS:
        case PROGTP_APP_ACTION_FORM_STATE_NEXT:
            break;
        case PROGTP_APP_ACTION_PAGE_PREVIOUS: PageRows(state, -1); break;
        case PROGTP_APP_ACTION_PAGE_NEXT: PageRows(state, 1); break;
        case PROGTP_APP_ACTION_FILTER_STATE_ALL: SetStateFilter(state, false, PROGTP_EQUIPMENT_OPERATIONAL); break;
        case PROGTP_APP_ACTION_FILTER_STATE_OPERATIONAL: SetStateFilter(state, true, PROGTP_EQUIPMENT_OPERATIONAL); break;
        case PROGTP_APP_ACTION_FILTER_STATE_FAILED: SetStateFilter(state, true, PROGTP_EQUIPMENT_FAILED); break;
        case PROGTP_APP_ACTION_FILTER_STATE_MAINTENANCE: SetStateFilter(state, true, PROGTP_EQUIPMENT_MAINTENANCE); break;
        case PROGTP_APP_ACTION_FILTER_STATE_DISABLED: SetStateFilter(state, true, PROGTP_EQUIPMENT_DISABLED); break;
        case PROGTP_APP_ACTION_FILTER_TYPE_ALL: SetTypeFilter(state, NULL); break;
        case PROGTP_APP_ACTION_FILTER_TYPE_PREVIOUS: CycleTypeFilter(state, -1); break;
        case PROGTP_APP_ACTION_FILTER_TYPE_NEXT: CycleTypeFilter(state, 1); break;
        case PROGTP_APP_ACTION_FILTER_STATE_PREVIOUS: CycleStateFilter(state, -1); break;
        case PROGTP_APP_ACTION_FILTER_STATE_NEXT: CycleStateFilter(state, 1); break;
        case PROGTP_APP_ACTION_CONNECTIVITY_PING_SELECTED:
            QueueConnectivityRequest(state, PROGTP_CONNECTIVITY_PING_SELECTED);
            break;
        case PROGTP_APP_ACTION_CONNECTIVITY_PING_ALL:
            QueueConnectivityRequest(state, PROGTP_CONNECTIVITY_PING_ALL);
            break;
        case PROGTP_APP_ACTION_CONNECTIVITY_COMMAND_FIELD:
            StartInput(state, PROGTP_APP_INPUT_CONNECTIVITY_COMMAND);
            break;
        case PROGTP_APP_ACTION_CONNECTIVITY_RUN_CUSTOM:
            QueueConnectivityRequest(state, PROGTP_CONNECTIVITY_CUSTOM);
            break;
        case PROGTP_APP_ACTION_CONNECTIVITY_PREVIOUS_TARGET: MoveInventorySelection(state, -1); break;
        case PROGTP_APP_ACTION_CONNECTIVITY_NEXT_TARGET: MoveInventorySelection(state, 1); break;
        case PROGTP_APP_ACTION_CONNECTIVITY_PAGE_PREVIOUS:
            if (state->connectivity_row_offset == 0) {
                size_t count = state->inventory.array.length;
                state->connectivity_row_offset = count == 0 ? 0 : ((count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
            } else {
                state->connectivity_row_offset = state->connectivity_row_offset > PROGTP_VISIBLE_ROWS
                    ? state->connectivity_row_offset - PROGTP_VISIBLE_ROWS
                    : 0;
            }
            break;
        case PROGTP_APP_ACTION_CONNECTIVITY_PAGE_NEXT:
            if (state->inventory.array.length > PROGTP_VISIBLE_ROWS) {
                size_t next = state->connectivity_row_offset + PROGTP_VISIBLE_ROWS;
                state->connectivity_row_offset = next < state->inventory.array.length ? next : 0;
            }
            break;
        case PROGTP_APP_ACTION_SENSOR_IMPORT: QueueSensorImportRequest(state); break;
        case PROGTP_APP_ACTION_SENSOR_PREVIOUS: MoveSensorSelection(state, -1); break;
        case PROGTP_APP_ACTION_SENSOR_NEXT: MoveSensorSelection(state, 1); break;
        case PROGTP_APP_ACTION_SENSOR_PAGE_PREVIOUS: PageSensors(state, -1); break;
        case PROGTP_APP_ACTION_SENSOR_PAGE_NEXT: PageSensors(state, 1); break;
        case PROGTP_APP_ACTION_SENSOR_FILTER_ALL:
            state->sensor_filter_anomalous = false;
            state->sensor_row_offset = 0;
            EnsureSensorSelectionInFilter(state);
            break;
        case PROGTP_APP_ACTION_SENSOR_FILTER_ANOMALOUS:
            state->sensor_filter_anomalous = true;
            state->sensor_row_offset = 0;
            EnsureSensorSelectionInFilter(state);
            break;
        case PROGTP_APP_ACTION_SENSOR_SEARCH_FIELD:
            state->input_mode = PROGTP_APP_INPUT_SENSOR_CODE;
            state->sensor_search_text[0] = '\0';
            SetStatus(state, "Sensor code: type value and press Enter");
            break;
        case PROGTP_APP_ACTION_SENSOR_CHOOSE_FILE:
            OpenSensorFileModal(state);
            SetStatus(state, "Enter the sensor file path");
            break;
        case PROGTP_APP_ACTION_NONE:
        case PROGTP_APP_ACTION_INPUT_BACKSPACE:
        case PROGTP_APP_ACTION_INPUT_SUBMIT:
            break;
    }
    EnsureSelection(state);
}

void ProgTP_AppHandleTextInput(ProgTP_AppState *state, uint32_t codepoint) {
    if (codepoint < 32u || codepoint > 126u) {
        return;
    }
    if (state->modal == PROGTP_APP_MODAL_ADD_EQUIPMENT || state->modal == PROGTP_APP_MODAL_UPDATE_EQUIPMENT) {
        size_t buffer_size = 0;
        char *buffer = FormFieldBuffer(state, state->form_field, &buffer_size);
        if (!buffer || buffer_size == 0) {
            return;
        }
        size_t length = strlen(buffer);
        if (length + 1u >= buffer_size) {
            return;
        }
        buffer[length] = (char)codepoint;
        buffer[length + 1u] = '\0';
        return;
    }
    if (state->modal == PROGTP_APP_MODAL_SENSOR_FILE) {
        size_t length = strlen(state->sensor_input_path);
        if (length + 1u >= sizeof(state->sensor_input_path)) {
            return;
        }
        state->sensor_input_path[length] = (char)codepoint;
        state->sensor_input_path[length + 1u] = '\0';
        return;
    }
    if (state->input_mode == PROGTP_APP_INPUT_NONE) {
        return;
    }
    char *buffer = state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND
        ? state->connectivity_custom_command
        : state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE ? state->sensor_search_text : state->input_text;
    size_t buffer_size = state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND
        ? sizeof(state->connectivity_custom_command)
        : state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE ? sizeof(state->sensor_search_text) : sizeof(state->input_text);
    size_t length = strlen(buffer);
    if (length + 1u >= buffer_size) {
        return;
    }
    buffer[length] = (char)codepoint;
    buffer[length + 1u] = '\0';
}

static void PrepareText(ProgTP_AppState *state) {
    if (state->active_module == 1) {
        ProgTP_EquipmentInventorySummary(&state->inventory, state->summary_text, sizeof(state->summary_text));
    } else if (state->active_module == 2) {
        snprintf(
            state->summary_text,
            sizeof(state->summary_text),
            "%s",
            "Execute ping tests, save raw command output, and log connectivity checks");
    } else if (state->active_module == 3) {
        snprintf(
            state->summary_text,
            sizeof(state->summary_text),
            "%s",
            "Import rack sensor readings, flag anomalies, and open technical incidents");
    } else {
        snprintf(
            state->summary_text,
            sizeof(state->summary_text),
            "%s",
            "Shared workspace for the practical assignment modules");
    }
    snprintf(state->title_text, sizeof(state->title_text), "Module %d - %s", state->active_module, ModuleName(state->active_module));
    snprintf(state->total_metric_text, sizeof(state->total_metric_text), "%zu devices", state->inventory.array.length);
    snprintf(state->selected_metric_text, sizeof(state->selected_metric_text), "#%u", state->selected_code);
    snprintf(state->type_filter_text, sizeof(state->type_filter_text), "%s", state->type_filter[0] != '\0' ? state->type_filter : "All types");
    snprintf(
        state->filter_metric_text,
        sizeof(state->filter_metric_text),
        "%s / %s",
        state->state_filter_enabled ? ProgTP_EquipmentStateName(state->state_filter) : "Any state",
        state->type_filter_text);
    for (int module = 1; module <= 8; ++module) {
        snprintf(state->module_labels[module - 1], sizeof(state->module_labels[module - 1]), "%d. %s", module, ModuleName(module));
    }
    ProgTP_Equipment *selected = SelectedEquipment(state);
    if (selected) {
        ProgTP_EquipmentFormatLine(selected, state->selected_text, sizeof(state->selected_text));
    } else {
        snprintf(state->selected_text, sizeof(state->selected_text), "No equipment selected");
    }
    if (state->input_mode != PROGTP_APP_INPUT_NONE) {
        const char *active_input = state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND
            ? state->connectivity_custom_command
            : state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE ? state->sensor_search_text : state->input_text;
        snprintf(state->help_text, sizeof(state->help_text), "%s: %.320s", InputModeName(state->input_mode), active_input);
        snprintf(
            state->search_display_text,
            sizeof(state->search_display_text),
            "%s%s",
            state->input_text[0] != '\0' ? state->input_text : SearchPlaceholder(state->input_mode),
            state->input_text[0] != '\0' ? "_" : "");
    } else {
        snprintf(state->help_text, sizeof(state->help_text), "%s workspace", ModuleName(state->active_module));
        snprintf(state->search_display_text, sizeof(state->search_display_text), "%s", SearchPlaceholder(PROGTP_APP_INPUT_NONE));
    }
    for (int field = 0; field < (int)PROGTP_APP_FORM_FIELD_COUNT; ++field) {
        size_t buffer_size = 0;
        char *value = FormFieldBuffer(state, (ProgTP_AppFormField)field, &buffer_size);
        (void)buffer_size;
        bool active = state->modal != PROGTP_APP_MODAL_NONE && state->form_field == (ProgTP_AppFormField)field;
        snprintf(
            state->form_display_text[field],
            sizeof(state->form_display_text[field]),
            "%s%s",
            value && value[0] != '\0' ? value : " ",
            active ? "_" : "");
    }
    PrepareRows(state);
    PrepareConnectivityText(state);
    PrepareSensorText(state);
}

static void MainModule(ProgTP_AppState *state) {
    if (state->active_module == 1) {
        InventoryModule(state);
    } else if (state->active_module == 2) {
        ConnectivityModule(state);
    } else if (state->active_module == 3) {
        SensorModule(state);
    } else {
        PlaceholderModule(state->active_module);
    }
}

Clay_RenderCommandArray ProgTP_AppBuildLayout(ProgTP_AppState *state, const char *target_name, float delta_time) {
    (void)delta_time;
    progtp_interaction_state = state;
    Clay_Dimensions layout_dimensions = Clay_GetLayoutDimensions();
    progtp_ui_compact = layout_dimensions.width < 900.0f;
    PrepareText(state);

    Clay_BeginLayout();

    CLAY(CLAY_ID("Root"), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .padding = CLAY_PADDING_ALL(0),
        },
        .backgroundColor = COLOR_PAGE,
    }) {
        CLAY(CLAY_ID("AppShell"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            },
            .backgroundColor = COLOR_PAGE,
        }) {
            CLAY(CLAY_ID("Sidebar"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_FIXED(progtp_ui_compact ? 170.0f : 240.0f), CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(progtp_ui_compact ? 10 : 14),
                    .childGap = progtp_ui_compact ? 6 : 8,
                },
                .backgroundColor = COLOR_SIDEBAR,
            }) {
                TextLine("Mini NOC", 24, COLOR_WHITE);
                TextLine("Modules", 13, COLOR_SURFACE_ALT);
                for (int module = 1; module <= 8; ++module) {
                    ModuleButton(state, module);
                }
            }
            CLAY(CLAY_ID("Workspace"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(progtp_ui_compact ? 10 : 18),
                    .childGap = progtp_ui_compact ? 10 : 14,
                },
                .backgroundColor = COLOR_SURFACE_ALT,
            }) {
                CLAY(CLAY_ID("TopBar"), {
                    .layout = {
                        .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                        .childGap = 12,
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                    },
                }) {
                    CLAY(CLAY_ID("TopTitle"), {
                        .layout = {
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                            .childGap = 4,
                        },
                    }) {
                        TextLine(state->title_text, 26, COLOR_TEXT);
                        TextLine(state->summary_text, 13, COLOR_MUTED);
                    }
                    CLAY(CLAY_ID("TargetBadge"), {
                        .layout = {
                            .sizing = { progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(ControlHeight()) },
                            .padding = { 12, 12, 0, 0 },
                            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = COLOR_ACCENT_DARK,
                        .cornerRadius = CLAY_CORNER_RADIUS(5),
                    }) {
                        TextLine(target_name, 13, COLOR_WHITE);
                    }
                }
                MainModule(state);
            }
        }
        ModalOverlay(state, layout_dimensions);
    }

    return Clay_EndLayout(delta_time);
}
