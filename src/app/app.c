#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t CStringLength(const char *value) {
    int32_t length = 0;
    while (value[length] != '\0') {
        ++length;
    }
    return length;
}

static const Clay_Color COLOR_PAGE = {25, 27, 30, 255};
static const Clay_Color COLOR_SIDEBAR = {35, 39, 45, 255};
static const Clay_Color COLOR_SURFACE = {246, 248, 250, 255};
static const Clay_Color COLOR_SURFACE_ALT = {235, 240, 245, 255};
static const Clay_Color COLOR_SURFACE_DARK = {223, 230, 237, 255};
static const Clay_Color COLOR_ACCENT = {36, 121, 108, 255};
static const Clay_Color COLOR_ACCENT_DARK = {24, 88, 80, 255};
static const Clay_Color COLOR_DANGER = {176, 54, 63, 255};
static const Clay_Color COLOR_TEXT = {22, 26, 31, 255};
static const Clay_Color COLOR_MUTED = {91, 99, 110, 255};
static const Clay_Color COLOR_WHITE = {255, 255, 255, 255};
static const Clay_Color COLOR_LINE = {206, 214, 222, 255};
static const Clay_Color COLOR_OVERLAY = {9, 12, 16, 178};

#define PROGTP_UI_MODULE_BASE 100u
#define PROGTP_UI_SELECT_BASE 1000u
#define PROGTP_UI_FORM_FIELD_BASE 3000u
#define PROGTP_UI_FORM_STATE_BASE 4000u
#define PROGTP_VISIBLE_ROWS 12u

static ProgTP_AppState *progtp_interaction_state;
static bool progtp_ui_compact;

static float ControlHeight(void) {
    return progtp_ui_compact ? 42.0f : 34.0f;
}

static float ButtonMinWidth(void) {
    return progtp_ui_compact ? 52.0f : 86.0f;
}

static uint16_t ControlGap(void) {
    return progtp_ui_compact ? 4u : 8u;
}

void ProgTP_HandleClayError(Clay_ErrorData errorData) {
#if !defined(CLAY_WASM) && !defined(PROGTP_WEB)
    fprintf(stderr, "Clay error: %.*s\n", (int)errorData.errorText.length, errorData.errorText.chars);
#else
    (void)errorData;
#endif
}

static Clay_String StringFromCString(const char *value) {
    return (Clay_String){
        .length = CStringLength(value),
        .chars = (char *)value,
        .isStaticallyAllocated = false,
    };
}

static const char *ModuleName(int module) {
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

static const char *InputModeName(ProgTP_AppInputMode mode) {
    switch (mode) {
        case PROGTP_APP_INPUT_SEARCH_CODE: return "Search code";
        case PROGTP_APP_INPUT_SEARCH_IP: return "Search IP";
        case PROGTP_APP_INPUT_SEARCH_MAC: return "Search MAC";
        case PROGTP_APP_INPUT_CONNECTIVITY_COMMAND: return "Custom command";
        case PROGTP_APP_INPUT_NONE: break;
    }
    return "";
}

static const char *SearchPlaceholder(ProgTP_AppInputMode mode) {
    switch (mode) {
        case PROGTP_APP_INPUT_SEARCH_CODE: return "Enter equipment code";
        case PROGTP_APP_INPUT_SEARCH_IP: return "Enter IP address";
        case PROGTP_APP_INPUT_SEARCH_MAC: return "Enter MAC address";
        case PROGTP_APP_INPUT_CONNECTIVITY_COMMAND: return "Enter a command to run";
        case PROGTP_APP_INPUT_NONE: break;
    }
    return "Select Code, IP, or MAC";
}

static ProgTP_Equipment *SelectedEquipment(ProgTP_AppState *state) {
    return ProgTP_EquipmentInventoryFindByCode(&state->inventory, state->selected_code);
}

static bool MatchesCurrentView(const ProgTP_AppState *state, const ProgTP_Equipment *equipment);
static void EnsureSelectionInCurrentView(ProgTP_AppState *state);

static void EnsureSelection(ProgTP_AppState *state) {
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

static void SetStatus(ProgTP_AppState *state, const char *message) {
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

static void MarkInventoryChanged(ProgTP_AppState *state) {
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

void ProgTP_AppInit(ProgTP_AppState *state, bool persistence_enabled, const char *storage_path) {
    memset(state, 0, sizeof(*state));
    state->active_module = 1;
    state->persistence_enabled = persistence_enabled;
    snprintf(
        state->connectivity_result.summary,
        sizeof(state->connectivity_result.summary),
        "%s",
        "No connectivity test has been run");
    snprintf(state->storage_path, sizeof(state->storage_path), "%s", storage_path ? storage_path : "equipamentos.dat");
    ProgTP_EquipmentInventoryInit(&state->inventory);
    LoadInventory(state);
}

void ProgTP_AppDestroy(ProgTP_AppState *state) {
    PersistIfEnabled(state);
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

static bool IsFirstTypeOccurrence(const ProgTP_AppState *state, size_t item_index) {
    const char *type = state->inventory.array.items[item_index].type;
    for (size_t i = 0; i < item_index; ++i) {
        if (ProgTP_TextEqualsIgnoreCase(state->inventory.array.items[i].type, type)) {
            return false;
        }
    }
    return true;
}

static size_t TypeFilterCount(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < state->inventory.array.length; ++i) {
        if (IsFirstTypeOccurrence(state, i)) {
            ++count;
        }
    }
    return count;
}

static const char *TypeFilterAt(const ProgTP_AppState *state, size_t type_index) {
    size_t current = 0;
    for (size_t i = 0; i < state->inventory.array.length; ++i) {
        if (!IsFirstTypeOccurrence(state, i)) {
            continue;
        }
        if (current == type_index) {
            return state->inventory.array.items[i].type;
        }
        ++current;
    }
    return NULL;
}

static void ResetRowNavigation(ProgTP_AppState *state) {
    state->row_offset = 0;
    EnsureSelectionInCurrentView(state);
}

static void SetStateFilter(ProgTP_AppState *state, bool enabled, ProgTP_EquipmentState equipment_state) {
    state->state_filter_enabled = enabled;
    state->state_filter = equipment_state;
    ResetRowNavigation(state);
}

static void CycleStateFilter(ProgTP_AppState *state, int direction) {
    int current = state->state_filter_enabled ? (int)state->state_filter + 1 : 0;
    int next = current + direction;
    if (next < 0) {
        next = 4;
    } else if (next > 4) {
        next = 0;
    }
    if (next == 0) {
        SetStateFilter(state, false, PROGTP_EQUIPMENT_OPERATIONAL);
    } else {
        SetStateFilter(state, true, (ProgTP_EquipmentState)(next - 1));
    }
}

static void SetTypeFilter(ProgTP_AppState *state, const char *type) {
    snprintf(state->type_filter, sizeof(state->type_filter), "%s", type ? type : "");
    ResetRowNavigation(state);
}

static void CycleTypeFilter(ProgTP_AppState *state, int direction) {
    size_t count = TypeFilterCount(state);
    if (count == 0) {
        SetTypeFilter(state, NULL);
        return;
    }

    size_t current = count;
    if (state->type_filter[0] != '\0') {
        for (size_t i = 0; i < count; ++i) {
            const char *type = TypeFilterAt(state, i);
            if (type && ProgTP_TextEqualsIgnoreCase(type, state->type_filter)) {
                current = i;
                break;
            }
        }
    }

    size_t next = 0;
    if (current == count) {
        next = direction > 0 ? 0 : count - 1u;
    } else if (direction > 0) {
        next = (current + 1u) % count;
    } else {
        next = current == 0 ? count - 1u : current - 1u;
    }
    SetTypeFilter(state, TypeFilterAt(state, next));
}

static size_t CountFilteredRows(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < state->inventory.array.length; ++i) {
        if (MatchesCurrentView(state, &state->inventory.array.items[i])) {
            ++count;
        }
    }
    return count;
}

static bool SelectFilteredRowAt(ProgTP_AppState *state, size_t filtered_index) {
    size_t index = 0;
    for (size_t i = 0; i < state->inventory.array.length; ++i) {
        ProgTP_Equipment *equipment = &state->inventory.array.items[i];
        if (!MatchesCurrentView(state, equipment)) {
            continue;
        }
        if (index == filtered_index) {
            state->selected_code = equipment->code;
            return true;
        }
        ++index;
    }
    return false;
}

static void EnsureSelectionInCurrentView(ProgTP_AppState *state) {
    ProgTP_Equipment *selected = SelectedEquipment(state);
    if (selected && MatchesCurrentView(state, selected)) {
        return;
    }
    if (!SelectFilteredRowAt(state, 0)) {
        EnsureSelection(state);
    }
}

static void MoveSelection(ProgTP_AppState *state, int direction) {
    size_t filtered_count = CountFilteredRows(state);
    if (filtered_count == 0) {
        state->selected_code = 0;
        return;
    }
    size_t current_index = 0;
    bool found = false;
    for (size_t i = 0; i < state->inventory.array.length; ++i) {
        ProgTP_Equipment *equipment = &state->inventory.array.items[i];
        if (!MatchesCurrentView(state, equipment)) {
            continue;
        }
        if (equipment->code == state->selected_code) {
            found = true;
            break;
        }
        ++current_index;
    }
    if (!found) {
        current_index = 0;
    }

    size_t next_index = current_index;
    if (direction > 0) {
        next_index = (current_index + 1u) % filtered_count;
    } else if (current_index == 0) {
        next_index = filtered_count - 1u;
    } else {
        next_index = current_index - 1u;
    }
    SelectFilteredRowAt(state, next_index);
}

static void MoveInventorySelection(ProgTP_AppState *state, int direction) {
    size_t count = state->inventory.array.length;
    if (count == 0) {
        state->selected_code = 0;
        return;
    }
    size_t current = 0;
    for (size_t i = 0; i < count; ++i) {
        if (state->inventory.array.items[i].code == state->selected_code) {
            current = i;
            break;
        }
    }
    size_t next;
    if (direction > 0) {
        next = (current + 1u) % count;
    } else {
        next = current == 0 ? count - 1u : current - 1u;
    }
    state->selected_code = state->inventory.array.items[next].code;
}

static char *FormFieldBuffer(ProgTP_AppState *state, ProgTP_AppFormField field, size_t *buffer_size) {
    switch (field) {
        case PROGTP_APP_FORM_NAME: *buffer_size = sizeof(state->form_name); return state->form_name;
        case PROGTP_APP_FORM_TYPE: *buffer_size = sizeof(state->form_type); return state->form_type;
        case PROGTP_APP_FORM_BRAND: *buffer_size = sizeof(state->form_brand); return state->form_brand;
        case PROGTP_APP_FORM_MODEL: *buffer_size = sizeof(state->form_model); return state->form_model;
        case PROGTP_APP_FORM_IP: *buffer_size = sizeof(state->form_ip); return state->form_ip;
        case PROGTP_APP_FORM_MAC: *buffer_size = sizeof(state->form_mac); return state->form_mac;
        case PROGTP_APP_FORM_LOCATION: *buffer_size = sizeof(state->form_location); return state->form_location;
        case PROGTP_APP_FORM_FIELD_COUNT: break;
    }
    *buffer_size = 0;
    return NULL;
}

static const char *FormFieldLabel(ProgTP_AppFormField field) {
    switch (field) {
        case PROGTP_APP_FORM_NAME: return "Name";
        case PROGTP_APP_FORM_TYPE: return "Type";
        case PROGTP_APP_FORM_BRAND: return "Brand";
        case PROGTP_APP_FORM_MODEL: return "Model";
        case PROGTP_APP_FORM_IP: return "IP address";
        case PROGTP_APP_FORM_MAC: return "MAC address";
        case PROGTP_APP_FORM_LOCATION: return "Location";
        case PROGTP_APP_FORM_FIELD_COUNT: break;
    }
    return "";
}

static void CloseModal(ProgTP_AppState *state) {
    state->modal = PROGTP_APP_MODAL_NONE;
    state->input_mode = PROGTP_APP_INPUT_NONE;
}

static void OpenAddModal(ProgTP_AppState *state) {
    state->modal = PROGTP_APP_MODAL_ADD_EQUIPMENT;
    state->input_mode = PROGTP_APP_INPUT_NONE;
    state->form_field = PROGTP_APP_FORM_NAME;
    state->form_state = PROGTP_EQUIPMENT_OPERATIONAL;
    state->form_pending = false;
    state->form_name[0] = '\0';
    snprintf(state->form_type, sizeof(state->form_type), "%s", "Router");
    state->form_brand[0] = '\0';
    state->form_model[0] = '\0';
    state->form_ip[0] = '\0';
    state->form_mac[0] = '\0';
    state->form_location[0] = '\0';
    SetStatus(state, "Add equipment: fill the form and press Save");
}

static void OpenUpdateModal(ProgTP_AppState *state) {
    ProgTP_Equipment *equipment = SelectedEquipment(state);
    if (!equipment) {
        SetStatus(state, "No equipment selected");
        return;
    }
    state->modal = PROGTP_APP_MODAL_UPDATE_EQUIPMENT;
    state->input_mode = PROGTP_APP_INPUT_NONE;
    state->form_field = PROGTP_APP_FORM_NAME;
    state->form_state = equipment->state;
    state->form_pending = equipment->has_pending_incidents;
    snprintf(state->form_name, sizeof(state->form_name), "%s", equipment->name);
    snprintf(state->form_type, sizeof(state->form_type), "%s", equipment->type);
    snprintf(state->form_brand, sizeof(state->form_brand), "%s", equipment->brand);
    snprintf(state->form_model, sizeof(state->form_model), "%s", equipment->model);
    snprintf(state->form_ip, sizeof(state->form_ip), "%s", equipment->ip_address);
    snprintf(state->form_mac, sizeof(state->form_mac), "%s", equipment->mac_address);
    snprintf(state->form_location, sizeof(state->form_location), "%s", equipment->location);
    snprintf(state->status, sizeof(state->status), "Update equipment #%u: edit the form and press Save", equipment->code);
}

static void OpenRemoveModal(ProgTP_AppState *state) {
    ProgTP_Equipment *equipment = SelectedEquipment(state);
    if (!equipment) {
        SetStatus(state, "No equipment selected");
        return;
    }
    state->modal = PROGTP_APP_MODAL_REMOVE_EQUIPMENT;
    state->input_mode = PROGTP_APP_INPUT_NONE;
    snprintf(state->modal_message, sizeof(state->modal_message), "Remove #%u - %s?", equipment->code, equipment->name);
    SetStatus(state, "Confirm removal");
}

static void SubmitEquipmentForm(ProgTP_AppState *state) {
    ProgTP_EquipmentInput input;
    ProgTP_EquipmentInputInit(
        &input,
        state->form_name,
        state->form_type,
        state->form_brand,
        state->form_model,
        state->form_ip,
        state->form_mac,
        state->form_location,
        state->form_state);

    char error[256] = {0};
    if (state->modal == PROGTP_APP_MODAL_ADD_EQUIPMENT) {
        ProgTP_Equipment created;
        if (ProgTP_EquipmentInventoryAdd(&state->inventory, &input, &created, error, sizeof(error))) {
            ProgTP_Equipment *added = ProgTP_EquipmentInventoryFindByCode(&state->inventory, created.code);
            if (added) {
                added->has_pending_incidents = state->form_pending;
            }
            state->selected_code = created.code;
            snprintf(state->status, sizeof(state->status), "Added %s", created.name);
            CloseModal(state);
            MarkInventoryChanged(state);
        } else {
            snprintf(state->status, sizeof(state->status), "Add failed: %s", error);
        }
    } else if (state->modal == PROGTP_APP_MODAL_UPDATE_EQUIPMENT) {
        uint32_t code = state->selected_code;
        if (ProgTP_EquipmentInventoryUpdate(&state->inventory, code, &input, error, sizeof(error))) {
            if (!ProgTP_EquipmentInventorySetPendingIncidents(&state->inventory, code, state->form_pending, error, sizeof(error))) {
                snprintf(state->status, sizeof(state->status), "Update failed: %s", error);
                return;
            }
            SetStatus(state, "Updated selected equipment");
            CloseModal(state);
            MarkInventoryChanged(state);
        } else {
            snprintf(state->status, sizeof(state->status), "Update failed: %s", error);
        }
    }
}

static void ConfirmRemoveSelected(ProgTP_AppState *state) {
    uint32_t code = state->selected_code;
    char error[256] = {0};
    if (ProgTP_EquipmentInventoryRemove(&state->inventory, code, error, sizeof(error))) {
        CloseModal(state);
        SetStatus(state, "Removed selected equipment");
        EnsureSelectionInCurrentView(state);
        MarkInventoryChanged(state);
    } else {
        snprintf(state->status, sizeof(state->status), "Remove failed: %s", error);
    }
}

static void AdvanceFormField(ProgTP_AppState *state, int direction) {
    int field = (int)state->form_field + direction;
    if (field < 0) {
        field = (int)PROGTP_APP_FORM_FIELD_COUNT - 1;
    } else if (field >= (int)PROGTP_APP_FORM_FIELD_COUNT) {
        field = 0;
    }
    state->form_field = (ProgTP_AppFormField)field;
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

static void QueueConnectivityRequest(ProgTP_AppState *state, ProgTP_ConnectivityOperation operation) {
    if (state->connectivity_request_pending || state->connectivity_request_in_flight) {
        SetStatus(state, "A connectivity command is already running");
        return;
    }
    if (operation != PROGTP_CONNECTIVITY_CUSTOM && !SelectedEquipment(state)) {
        SetStatus(state, "Select equipment before running ping");
        return;
    }
    if (operation == PROGTP_CONNECTIVITY_CUSTOM && state->connectivity_custom_command[0] == '\0') {
        SetStatus(state, "Enter a custom command first");
        state->input_mode = PROGTP_APP_INPUT_CONNECTIVITY_COMMAND;
        return;
    }
    memset(&state->connectivity_request, 0, sizeof(state->connectivity_request));
    state->connectivity_request.operation = operation;
    state->connectivity_request.equipment_code = state->selected_code;
    snprintf(
        state->connectivity_request.custom_command,
        sizeof(state->connectivity_request.custom_command),
        "%s",
        state->connectivity_custom_command);
    state->connectivity_request_pending = true;
    state->input_mode = PROGTP_APP_INPUT_NONE;
    SetStatus(state, operation == PROGTP_CONNECTIVITY_CUSTOM ? "Custom command queued" : "Ping test queued");
}

static void SubmitInput(ProgTP_AppState *state) {
    if (state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND) {
        QueueConnectivityRequest(state, PROGTP_CONNECTIVITY_CUSTOM);
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

static void BackspaceFormField(ProgTP_AppState *state) {
    size_t buffer_size = 0;
    char *buffer = FormFieldBuffer(state, state->form_field, &buffer_size);
    (void)buffer_size;
    if (!buffer) {
        return;
    }
    size_t length = strlen(buffer);
    if (length > 0) {
        buffer[length - 1u] = '\0';
    }
}

static void SubmitModal(ProgTP_AppState *state) {
    if (state->modal == PROGTP_APP_MODAL_REMOVE_EQUIPMENT) {
        ConfirmRemoveSelected(state);
    } else if (state->modal == PROGTP_APP_MODAL_ADD_EQUIPMENT || state->modal == PROGTP_APP_MODAL_UPDATE_EQUIPMENT) {
        SubmitEquipmentForm(state);
    }
}

static bool HandleModalAction(ProgTP_AppState *state, ProgTP_AppAction action) {
    if (state->modal == PROGTP_APP_MODAL_NONE) {
        return false;
    }
    switch (action) {
        case PROGTP_APP_ACTION_INPUT_BACKSPACE: BackspaceFormField(state); return true;
        case PROGTP_APP_ACTION_INPUT_SUBMIT:
        case PROGTP_APP_ACTION_FORM_SUBMIT: SubmitModal(state); return true;
        case PROGTP_APP_ACTION_FORM_CANCEL: CloseModal(state); SetStatus(state, "Canceled"); return true;
        case PROGTP_APP_ACTION_FORM_NEXT_FIELD: AdvanceFormField(state, 1); return true;
        case PROGTP_APP_ACTION_FORM_PREVIOUS_FIELD: AdvanceFormField(state, -1); return true;
        case PROGTP_APP_ACTION_FORM_TOGGLE_PENDING: state->form_pending = !state->form_pending; return true;
        case PROGTP_APP_ACTION_FORM_STATE_PREVIOUS:
            state->form_state = state->form_state == PROGTP_EQUIPMENT_OPERATIONAL
                ? PROGTP_EQUIPMENT_DISABLED
                : (ProgTP_EquipmentState)((int)state->form_state - 1);
            return true;
        case PROGTP_APP_ACTION_FORM_STATE_NEXT:
            state->form_state = (ProgTP_EquipmentState)(((int)state->form_state + 1) % 4);
            return true;
        case PROGTP_APP_ACTION_NONE: return false;
        default: return true;
    }
}

static void PageRows(ProgTP_AppState *state, int direction) {
    size_t filtered_count = CountFilteredRows(state);
    if (filtered_count <= PROGTP_VISIBLE_ROWS) {
        state->row_offset = 0;
        return;
    }
    if (direction > 0) {
        size_t next = state->row_offset + PROGTP_VISIBLE_ROWS;
        if (next >= filtered_count) {
            next = 0;
        }
        state->row_offset = next;
    } else if (state->row_offset == 0) {
        state->row_offset = ((filtered_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    } else {
        state->row_offset = state->row_offset > PROGTP_VISIBLE_ROWS ? state->row_offset - PROGTP_VISIBLE_ROWS : 0;
    }
    SelectFilteredRowAt(state, state->row_offset);
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
                : state->input_text;
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

    switch (action) {
        case PROGTP_APP_ACTION_NEXT: MoveSelection(state, 1); break;
        case PROGTP_APP_ACTION_PREVIOUS: MoveSelection(state, -1); break;
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
        case PROGTP_APP_ACTION_SEARCH_CODE: StartInput(state, PROGTP_APP_INPUT_SEARCH_CODE); break;
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
    if (state->input_mode == PROGTP_APP_INPUT_NONE) {
        return;
    }
    char *buffer = state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND
        ? state->connectivity_custom_command
        : state->input_text;
    size_t buffer_size = state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND
        ? sizeof(state->connectivity_custom_command)
        : sizeof(state->input_text);
    size_t length = strlen(buffer);
    if (length + 1u >= buffer_size) {
        return;
    }
    buffer[length] = (char)codepoint;
    buffer[length + 1u] = '\0';
}

static bool MatchesCurrentView(const ProgTP_AppState *state, const ProgTP_Equipment *equipment) {
    if (state->state_filter_enabled && equipment->state != state->state_filter) {
        return false;
    }
    if (state->type_filter[0] != '\0' && !ProgTP_TextEqualsIgnoreCase(equipment->type, state->type_filter)) {
        return false;
    }
    return true;
}

static void AddRowText(ProgTP_AppState *state, const ProgTP_Equipment *equipment) {
    if (!equipment || state->row_count >= sizeof(state->row_texts) / sizeof(state->row_texts[0])) {
        return;
    }
    char line[448];
    ProgTP_EquipmentFormatLine(equipment, line, sizeof(line));
    snprintf(
        state->row_texts[state->row_count],
        sizeof(state->row_texts[state->row_count]),
        "%c %s",
        equipment->code == state->selected_code ? '>' : ' ',
        line);
    state->row_codes[state->row_count] = equipment->code;
    ++state->row_count;
}

static void PrepareRows(ProgTP_AppState *state) {
    state->row_count = 0;
    state->filtered_count = 0;
    size_t selected_index = (size_t)-1;
    for (size_t i = 0; i < state->inventory.array.length; ++i) {
        if (MatchesCurrentView(state, &state->inventory.array.items[i])) {
            if (state->inventory.array.items[i].code == state->selected_code) {
                selected_index = state->filtered_count;
            }
            ++state->filtered_count;
        }
    }
    if (state->filtered_count == 0) {
        state->row_offset = 0;
        snprintf(state->row_page_text, sizeof(state->row_page_text), "No items in this view");
        return;
    }
    if (state->row_offset >= state->filtered_count) {
        state->row_offset = ((state->filtered_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }
    if (selected_index != (size_t)-1) {
        if (selected_index < state->row_offset || selected_index >= state->row_offset + PROGTP_VISIBLE_ROWS) {
            state->row_offset = (selected_index / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
        }
    }

    size_t filtered_index = 0;
    for (size_t i = 0; i < state->inventory.array.length; ++i) {
        if (!MatchesCurrentView(state, &state->inventory.array.items[i])) {
            continue;
        }
        if (filtered_index >= state->row_offset && state->row_count < PROGTP_VISIBLE_ROWS) {
            AddRowText(state, &state->inventory.array.items[i]);
        }
        ++filtered_index;
    }
    size_t first = state->row_offset + 1u;
    size_t last = state->row_offset + state->row_count;
    snprintf(state->row_page_text, sizeof(state->row_page_text), "Showing %zu-%zu of %zu", first, last, state->filtered_count);
}

static void PrepareConnectivityOutputLines(ProgTP_AppState *state) {
    state->connectivity_output_line_count = 0;
    const char *cursor = state->connectivity_result.output_preview;
    if (cursor[0] == '\0') {
        snprintf(
            state->connectivity_output_lines[0],
            sizeof(state->connectivity_output_lines[0]),
            "%.190s",
            state->connectivity_result.summary);
        state->connectivity_output_line_count = 1;
        return;
    }
    while (*cursor && state->connectivity_output_line_count < 8u) {
        char *line = state->connectivity_output_lines[state->connectivity_output_line_count];
        size_t line_size = sizeof(state->connectivity_output_lines[0]);
        size_t consumed = 0;
        size_t output_length = 0;
        while (cursor[consumed] && cursor[consumed] != '\n' && output_length + 1u < line_size) {
            if (cursor[consumed] != '\r') {
                line[output_length++] = cursor[consumed];
            }
            ++consumed;
        }
        line[output_length] = '\0';
        ++state->connectivity_output_line_count;
        cursor += consumed;
        while (*cursor && *cursor != '\n') {
            ++cursor;
        }
        while (*cursor == '\r' || *cursor == '\n') {
            ++cursor;
        }
    }
}

static void PrepareConnectivityText(ProgTP_AppState *state) {
    ProgTP_Equipment *selected = SelectedEquipment(state);
    if (selected) {
        snprintf(
            state->connectivity_target_text,
            sizeof(state->connectivity_target_text),
            "#%u %s | %s",
            selected->code,
            selected->name,
            selected->ip_address);
    } else {
        snprintf(state->connectivity_target_text, sizeof(state->connectivity_target_text), "%s", "No equipment selected");
    }
    snprintf(
        state->connectivity_status_text,
        sizeof(state->connectivity_status_text),
        "%s",
        state->connectivity_request_in_flight || state->connectivity_request_pending
            ? "Running"
            : state->connectivity_has_result ? (state->connectivity_result.command_succeeded ? "Completed" : "Failed") : "Ready");
    snprintf(
        state->connectivity_counts_text,
        sizeof(state->connectivity_counts_text),
        "%u run / %u replied / %u failed",
        state->connectivity_result.executed_count,
        state->connectivity_result.responded_count,
        state->connectivity_result.failed_count);
    snprintf(
        state->connectivity_command_display,
        sizeof(state->connectivity_command_display),
        "%s%s",
        state->connectivity_custom_command[0] != '\0'
            ? state->connectivity_custom_command
            : "Enter a command, for example: nslookup {ip}",
        state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND ? "_" : "");

    size_t count = state->inventory.array.length;
    if (count == 0) {
        state->connectivity_row_offset = 0;
    } else if (state->connectivity_row_offset >= count) {
        state->connectivity_row_offset = ((count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }
    size_t selected_index = (size_t)-1;
    for (size_t i = 0; i < count; ++i) {
        if (state->inventory.array.items[i].code == state->selected_code) {
            selected_index = i;
            break;
        }
    }
    if (selected_index != (size_t)-1 &&
        (selected_index < state->connectivity_row_offset ||
         selected_index >= state->connectivity_row_offset + PROGTP_VISIBLE_ROWS)) {
        state->connectivity_row_offset = (selected_index / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }
    state->connectivity_row_count = 0;
    for (size_t i = state->connectivity_row_offset;
         i < count && state->connectivity_row_count < PROGTP_VISIBLE_ROWS;
         ++i) {
        const ProgTP_Equipment *equipment = &state->inventory.array.items[i];
        size_t row = state->connectivity_row_count++;
        state->connectivity_row_codes[row] = equipment->code;
        snprintf(
            state->connectivity_row_texts[row],
            sizeof(state->connectivity_row_texts[row]),
            "#%u | %s | %s | %s",
            equipment->code,
            equipment->name,
            equipment->ip_address,
            ProgTP_EquipmentStateName(equipment->state));
    }
    if (count == 0) {
        snprintf(state->connectivity_row_page_text, sizeof(state->connectivity_row_page_text), "%s", "No equipment");
    } else {
        snprintf(
            state->connectivity_row_page_text,
            sizeof(state->connectivity_row_page_text),
            "Showing %zu-%zu of %zu",
            state->connectivity_row_offset + 1u,
            state->connectivity_row_offset + state->connectivity_row_count,
            count);
    }
    PrepareConnectivityOutputLines(state);
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
            : state->input_text;
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
}

static void TextLine(const char *text, uint16_t size, Clay_Color color) {
    CLAY_TEXT(StringFromCString(text), CLAY_TEXT_CONFIG({
        .fontSize = size,
        .textColor = color,
    }));
}

static void HandleUiInteraction(Clay_ElementId element_id, Clay_PointerData pointer_data, void *user_data) {
    (void)element_id;
    if (!progtp_interaction_state || pointer_data.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        return;
    }
    uintptr_t value = (uintptr_t)user_data;
    if (value >= PROGTP_UI_FORM_STATE_BASE) {
        uint32_t state_value = (uint32_t)(value - PROGTP_UI_FORM_STATE_BASE);
        if (state_value <= (uint32_t)PROGTP_EQUIPMENT_DISABLED) {
            progtp_interaction_state->form_state = (ProgTP_EquipmentState)state_value;
        }
        return;
    }
    if (value >= PROGTP_UI_FORM_FIELD_BASE) {
        ProgTP_AppFormField field = (ProgTP_AppFormField)(value - PROGTP_UI_FORM_FIELD_BASE);
        if (field >= 0 && field < PROGTP_APP_FORM_FIELD_COUNT) {
            progtp_interaction_state->form_field = field;
        }
        return;
    }
    if (value >= PROGTP_UI_SELECT_BASE) {
        progtp_interaction_state->selected_code = (uint32_t)(value - PROGTP_UI_SELECT_BASE);
        EnsureSelection(progtp_interaction_state);
        return;
    }
    if (value >= PROGTP_UI_MODULE_BASE) {
        progtp_interaction_state->active_module = (int)(value - PROGTP_UI_MODULE_BASE);
        progtp_interaction_state->input_mode = PROGTP_APP_INPUT_NONE;
        snprintf(
            progtp_interaction_state->status,
            sizeof(progtp_interaction_state->status),
            "Opened Module %d - %s",
            progtp_interaction_state->active_module,
            ModuleName(progtp_interaction_state->active_module));
        return;
    }
    ProgTP_AppHandleAction(progtp_interaction_state, (ProgTP_AppAction)value);
}

static void AttachInteraction(uintptr_t interaction) {
    Clay_OnHover(HandleUiInteraction, (void *)interaction); /* NOLINT(performance-no-int-to-ptr): Clay stores small integer interaction ids as opaque user data. */
}

static void Button(uint32_t id, const char *label, uintptr_t interaction, bool active, bool danger) {
    Clay_Color color = active ? COLOR_ACCENT : COLOR_SURFACE_DARK;
    Clay_Color text_color = active ? COLOR_WHITE : COLOR_TEXT;
    if (danger) {
        color = COLOR_DANGER;
        text_color = COLOR_WHITE;
    }
    CLAY(CLAY_IDI("Button", id), {
        .layout = {
            .sizing = { CLAY_SIZING_FIT(.min = ButtonMinWidth()), CLAY_SIZING_FIXED(ControlHeight()) },
            .padding = { 12, 12, 0, 0 },
            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = color,
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        AttachInteraction(interaction);
        TextLine(label, 13, text_color);
    }
}

static void FormTextField(uint32_t id, ProgTP_AppState *state, ProgTP_AppFormField field) {
    bool active = state->form_field == field;
    CLAY(CLAY_IDI("FormField", id), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .childGap = 5,
        },
    }) {
        TextLine(FormFieldLabel(field), 12, COLOR_MUTED);
        CLAY(CLAY_IDI("FormInput", id), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(38) },
                .padding = { 11, 11, 0, 0 },
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = active ? COLOR_WHITE : COLOR_SURFACE_ALT,
            .border = {
                .color = active ? COLOR_ACCENT : COLOR_LINE,
                .width = {
                    state->terminal_rendering ? 0 : 1,
                    state->terminal_rendering ? 0 : 1,
                    state->terminal_rendering ? 0 : 1,
                    state->terminal_rendering ? 0 : 1,
                    0,
                },
            },
            .cornerRadius = CLAY_CORNER_RADIUS(6),
        }) {
            AttachInteraction(PROGTP_UI_FORM_FIELD_BASE + (uintptr_t)field);
            TextLine(state->form_display_text[field], 13, COLOR_TEXT);
        }
    }
}

static void StateButton(uint32_t id, ProgTP_AppState *state, ProgTP_EquipmentState equipment_state) {
    Button(id, ProgTP_EquipmentStateName(equipment_state), PROGTP_UI_FORM_STATE_BASE + (uintptr_t)equipment_state, state->form_state == equipment_state, false);
}

static void ModuleButton(ProgTP_AppState *state, int module) {
    bool active = state->active_module == module;
    CLAY(CLAY_IDI("ModuleButton", (uint32_t)module), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(38) },
            .padding = { 12, 10, 0, 0 },
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = active ? COLOR_ACCENT : COLOR_SIDEBAR,
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        AttachInteraction(PROGTP_UI_MODULE_BASE + (uintptr_t)module);
        TextLine(state->module_labels[module - 1], 13, active ? COLOR_WHITE : COLOR_SURFACE_ALT);
    }
}

static void Metric(const char *label, const char *value) {
    CLAY(CLAY_IDI("Metric", (uint32_t)CStringLength(label) * 17u + (uint32_t)CStringLength(value)), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .padding = CLAY_PADDING_ALL(10),
            .childGap = 4,
        },
        .backgroundColor = COLOR_SURFACE_ALT,
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        TextLine(label, 12, COLOR_MUTED);
        TextLine(value, 17, COLOR_TEXT);
    }
}

static void Toolbar(ProgTP_AppState *state) {
    (void)state;
    CLAY(CLAY_ID("InventoryToolbar"), {
        .layout = {
            .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .childGap = ControlGap(),
        },
    }) {
        CLAY(CLAY_ID("InventoryToolbarPrimary"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(1, "Add", PROGTP_APP_ACTION_ADD_SAMPLE, false, false);
            Button(2, "Update", PROGTP_APP_ACTION_UPDATE_SELECTED, false, false);
            Button(3, "Remove", PROGTP_APP_ACTION_REMOVE_SELECTED, false, true);
            Button(4, "Prev", PROGTP_APP_ACTION_PREVIOUS, false, false);
            Button(5, "Next", PROGTP_APP_ACTION_NEXT, false, false);
        }
        CLAY(CLAY_ID("InventoryToolbarSecondary"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(8, "Save", PROGTP_APP_ACTION_SAVE, false, false);
            Button(9, "Load", PROGTP_APP_ACTION_LOAD, false, false);
        }
    }
}

static void FilterMenu(ProgTP_AppState *state) {
    CLAY(CLAY_ID("FilterMenu"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .childGap = 7,
        },
    }) {
        CLAY(CLAY_ID("FilterHeader"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            TextLine("Filters", 14, COLOR_TEXT);
            CLAY(CLAY_ID("FilterHeaderSpacer"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) } },
            }) {}
            Button(20, "Reset", PROGTP_APP_ACTION_FILTER_ALL, !state->state_filter_enabled && state->type_filter[0] == '\0', false);
        }
        CLAY(CLAY_ID("StateFilters"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            TextLine("State", 12, COLOR_MUTED);
            if (progtp_ui_compact) {
                Button(21, "Prev", PROGTP_APP_ACTION_FILTER_STATE_PREVIOUS, false, false);
                CLAY(CLAY_ID("StateFilterValue"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(.min = 120.0f), CLAY_SIZING_FIXED(ControlHeight()) },
                        .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = state->state_filter_enabled ? COLOR_WHITE : COLOR_SURFACE_ALT,
                    .border = {
                        .color = state->state_filter_enabled ? COLOR_ACCENT : COLOR_LINE,
                        .width = {
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            0,
                        },
                    },
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                }) {
                    TextLine(state->state_filter_enabled ? ProgTP_EquipmentStateName(state->state_filter) : "Any state", 13, COLOR_TEXT);
                }
                Button(22, "Next", PROGTP_APP_ACTION_FILTER_STATE_NEXT, false, false);
                Button(23, "Any", PROGTP_APP_ACTION_FILTER_STATE_ALL, !state->state_filter_enabled, false);
            } else {
                Button(21, "Any", PROGTP_APP_ACTION_FILTER_STATE_ALL, !state->state_filter_enabled, false);
                Button(22, "Operational", PROGTP_APP_ACTION_FILTER_STATE_OPERATIONAL, state->state_filter_enabled && state->state_filter == PROGTP_EQUIPMENT_OPERATIONAL, false);
                Button(23, "Failed", PROGTP_APP_ACTION_FILTER_STATE_FAILED, state->state_filter_enabled && state->state_filter == PROGTP_EQUIPMENT_FAILED, false);
                Button(24, "Maintenance", PROGTP_APP_ACTION_FILTER_STATE_MAINTENANCE, state->state_filter_enabled && state->state_filter == PROGTP_EQUIPMENT_MAINTENANCE, false);
                Button(25, "Disabled", PROGTP_APP_ACTION_FILTER_STATE_DISABLED, state->state_filter_enabled && state->state_filter == PROGTP_EQUIPMENT_DISABLED, false);
            }
        }
        CLAY(CLAY_ID("TypeFilters"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            TextLine("Type", 12, COLOR_MUTED);
            Button(26, "Prev", PROGTP_APP_ACTION_FILTER_TYPE_PREVIOUS, false, false);
            CLAY(CLAY_ID("TypeFilterValue"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(.min = 110.0f), CLAY_SIZING_FIXED(ControlHeight()) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = state->type_filter[0] != '\0' ? COLOR_WHITE : COLOR_SURFACE_ALT,
                .border = {
                    .color = state->type_filter[0] != '\0' ? COLOR_ACCENT : COLOR_LINE,
                    .width = {
                        state->terminal_rendering ? 0 : 1,
                        state->terminal_rendering ? 0 : 1,
                        state->terminal_rendering ? 0 : 1,
                        state->terminal_rendering ? 0 : 1,
                        0,
                    },
                },
                .cornerRadius = CLAY_CORNER_RADIUS(5),
            }) {
                TextLine(state->type_filter_text, 13, COLOR_TEXT);
            }
            Button(27, "Next", PROGTP_APP_ACTION_FILTER_TYPE_NEXT, false, false);
            Button(28, "Any", PROGTP_APP_ACTION_FILTER_TYPE_ALL, state->type_filter[0] == '\0', false);
        }
    }
}

static void SearchMenu(ProgTP_AppState *state) {
    CLAY(CLAY_ID("SearchMenu"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .childGap = 7,
        },
    }) {
        TextLine("Search", 14, COLOR_TEXT);
        CLAY(CLAY_ID("SearchControls"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            Button(30, "Code", PROGTP_APP_ACTION_SEARCH_CODE, state->input_mode == PROGTP_APP_INPUT_SEARCH_CODE, false);
            Button(31, "IP", PROGTP_APP_ACTION_SEARCH_IP, state->input_mode == PROGTP_APP_INPUT_SEARCH_IP, false);
            Button(32, "MAC", PROGTP_APP_ACTION_SEARCH_MAC, state->input_mode == PROGTP_APP_INPUT_SEARCH_MAC, false);
            CLAY(CLAY_ID("SearchInput"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(.min = progtp_ui_compact ? 150.0f : 180.0f), CLAY_SIZING_FIXED(ControlHeight()) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = COLOR_SURFACE,
                .border = {
                    .color = state->input_mode == PROGTP_APP_INPUT_NONE ? COLOR_LINE : COLOR_ACCENT,
                    .width = {
                        state->terminal_rendering ? 0 : 1,
                        state->terminal_rendering ? 0 : 1,
                        state->terminal_rendering ? 0 : 1,
                        state->terminal_rendering ? 0 : 1,
                        0,
                    },
                },
                .cornerRadius = CLAY_CORNER_RADIUS(5),
            }) {
                AttachInteraction(PROGTP_APP_ACTION_SEARCH_FIELD);
                TextLine(state->search_display_text, 13, state->input_mode == PROGTP_APP_INPUT_NONE ? COLOR_MUTED : COLOR_TEXT);
            }
            Button(33, "Submit", PROGTP_APP_ACTION_INPUT_SUBMIT, false, false);
        }
    }
}

static void InventoryRows(ProgTP_AppState *state) {
    CLAY(CLAY_ID("EquipmentTable"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(.min = 220) },
            .childGap = 2,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        CLAY(CLAY_ID("EquipmentTableHeader"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight() + 8.0f) },
                .padding = { 10, 10, 4, 4 },
                .childGap = 8,
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = COLOR_SURFACE_DARK,
        }) {
            CLAY(CLAY_ID("EquipmentTableTitle"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 2,
                },
            }) {
                TextLine("Equipment list", 13, COLOR_TEXT);
                TextLine(state->row_page_text, 12, COLOR_MUTED);
            }
            Button(34, "Prev Page", PROGTP_APP_ACTION_PAGE_PREVIOUS, false, false);
            Button(35, "Next Page", PROGTP_APP_ACTION_PAGE_NEXT, false, false);
        }
        for (size_t i = 0; i < state->row_count; ++i) {
            bool selected = state->row_codes[i] == state->selected_code;
            CLAY(CLAY_IDI("EquipmentRow", (uint32_t)i), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = selected ? COLOR_ACCENT : (i % 2u == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
            }) {
                AttachInteraction(PROGTP_UI_SELECT_BASE + (uintptr_t)state->row_codes[i]);
                TextLine(state->row_texts[i], 12, selected ? COLOR_WHITE : COLOR_TEXT);
            }
        }
    }
}

static void InventoryModule(ProgTP_AppState *state) {
    CLAY(CLAY_ID("InventoryModule"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = 12,
        },
    }) {
        CLAY(CLAY_ID("InventoryMetrics"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Inventory", state->total_metric_text);
            Metric("Selected", state->selected_metric_text);
            Metric("Filters", state->filter_metric_text);
        }
        Toolbar(state);
        FilterMenu(state);
        SearchMenu(state);
        CLAY(CLAY_ID("InventoryContent"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            InventoryRows(state);
            if (!progtp_ui_compact) {
                CLAY(CLAY_ID("SelectedEquipment"), {
                    .layout = {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(14),
                        .childGap = 10,
                    },
                    .backgroundColor = COLOR_SURFACE,
                    .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                }) {
                    TextLine("Selected equipment", 16, COLOR_TEXT);
                    TextLine(state->selected_text, 13, COLOR_MUTED);
                    if (state->status[0] != '\0') {
                        TextLine(state->status, 13, COLOR_DANGER);
                    }
                }
            }
        }
    }
}

static void ConnectivityEquipmentRows(ProgTP_AppState *state) {
    CLAY(CLAY_ID("ConnectivityEquipmentTable"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_GROW(.min = 360.0f),
                CLAY_SIZING_GROW(.min = 180.0f),
            },
            .childGap = 2,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        CLAY(CLAY_ID("ConnectivityEquipmentHeader"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight() + 8.0f) },
                .padding = { 10, 10, 4, 4 },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = COLOR_SURFACE_DARK,
        }) {
            CLAY(CLAY_ID("ConnectivityEquipmentTitle"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 2,
                },
            }) {
                TextLine("Ping targets", 13, COLOR_TEXT);
                TextLine(state->connectivity_row_page_text, 12, COLOR_MUTED);
            }
            Button(410, "Prev Page", PROGTP_APP_ACTION_CONNECTIVITY_PAGE_PREVIOUS, false, false);
            Button(411, "Next Page", PROGTP_APP_ACTION_CONNECTIVITY_PAGE_NEXT, false, false);
        }
        for (size_t i = 0; i < state->connectivity_row_count; ++i) {
            bool selected = state->connectivity_row_codes[i] == state->selected_code;
            CLAY(CLAY_IDI("ConnectivityEquipmentRow", (uint32_t)i), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = selected ? COLOR_ACCENT : (i % 2u == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
            }) {
                AttachInteraction(PROGTP_UI_SELECT_BASE + (uintptr_t)state->connectivity_row_codes[i]);
                TextLine(state->connectivity_row_texts[i], 12, selected ? COLOR_WHITE : COLOR_TEXT);
            }
        }
    }
}

static void ConnectivityResultPanel(ProgTP_AppState *state) {
    CLAY(CLAY_ID("ConnectivityResultPanel"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(380.0f),
                CLAY_SIZING_GROW(.min = 180.0f),
            },
            .padding = CLAY_PADDING_ALL(14),
            .childGap = 7,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        TextLine("Latest result", 16, COLOR_TEXT);
        TextLine(state->connectivity_result.summary, 13, state->connectivity_result.failed_count > 0 ? COLOR_DANGER : COLOR_MUTED);
        if (state->connectivity_has_result) {
            TextLine(state->connectivity_result.command, 12, COLOR_ACCENT_DARK);
            TextLine(state->connectivity_result.output_path, 12, COLOR_MUTED);
            for (size_t i = 0; i < state->connectivity_output_line_count; ++i) {
                TextLine(state->connectivity_output_lines[i], 12, COLOR_TEXT);
            }
        }
    }
}

static void ConnectivityModule(ProgTP_AppState *state) {
    CLAY(CLAY_ID("ConnectivityModule"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = progtp_ui_compact ? 8 : 12,
        },
    }) {
        CLAY(CLAY_ID("ConnectivityMetrics"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Target", state->connectivity_target_text);
            Metric("Execution", state->connectivity_status_text);
            Metric("Results", state->connectivity_counts_text);
        }
        CLAY(CLAY_ID("ConnectivityActions"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            CLAY(CLAY_ID("ConnectivityPingActions"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = ControlGap(),
                },
            }) {
                Button(400, "Ping Selected", PROGTP_APP_ACTION_CONNECTIVITY_PING_SELECTED, true, false);
                Button(401, "Ping All", PROGTP_APP_ACTION_CONNECTIVITY_PING_ALL, false, false);
            }
            CLAY(CLAY_ID("ConnectivityTargetActions"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = ControlGap(),
                    .childAlignment = { .x = CLAY_ALIGN_X_RIGHT },
                },
            }) {
                Button(402, "Prev Target", PROGTP_APP_ACTION_CONNECTIVITY_PREVIOUS_TARGET, false, false);
                Button(403, "Next Target", PROGTP_APP_ACTION_CONNECTIVITY_NEXT_TARGET, false, false);
            }
        }
        CLAY(CLAY_ID("CustomCommandSection"), {
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 6,
            },
        }) {
            TextLine("Custom command", 14, COLOR_TEXT);
            CLAY(CLAY_ID("CustomCommandControls"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = ControlGap(),
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
            }) {
                CLAY(CLAY_ID("CustomCommandInput"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(.min = 220.0f), CLAY_SIZING_FIXED(ControlHeight()) },
                        .padding = { 10, 10, 0, 0 },
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = COLOR_SURFACE,
                    .border = {
                        .color = state->input_mode == PROGTP_APP_INPUT_CONNECTIVITY_COMMAND ? COLOR_ACCENT : COLOR_LINE,
                        .width = {
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            0,
                        },
                    },
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                }) {
                    AttachInteraction(PROGTP_APP_ACTION_CONNECTIVITY_COMMAND_FIELD);
                    TextLine(
                        state->connectivity_command_display,
                        13,
                        state->connectivity_custom_command[0] == '\0' ? COLOR_MUTED : COLOR_TEXT);
                }
                Button(404, "Run Custom", PROGTP_APP_ACTION_CONNECTIVITY_RUN_CUSTOM, false, false);
            }
        }
        CLAY(CLAY_ID("ConnectivityContent"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            ConnectivityEquipmentRows(state);
            ConnectivityResultPanel(state);
        }
    }
}

static const char *ModuleFileName(int module) {
    static const char *file_names[] = {
        "",
        "equipamentos.dat",
        "resultado_ping.txt / log_monitorizacao.txt",
        "sensores_rack.txt / log_sensores.txt",
        "incidentes.dat",
        "configuracoes.dat",
        "relatorio_estado_rede_mes_ano.txt",
        "tecnicos.dat",
        "settings.dat",
    };
    return module >= 1 && module <= 8 ? file_names[module] : "";
}

static const char *ModuleTableTitle(int module) {
    static const char *titles[] = {
        "",
        "Equipment",
        "Connectivity checks",
        "Sensor readings",
        "Incident queue",
        "Configuration history",
        "Generated reports",
        "Technician registry",
        "Runtime settings",
    };
    return module >= 1 && module <= 8 ? titles[module] : "Records";
}

static const char *ModulePrimaryAction(int module) {
    static const char *actions[] = {
        "",
        "Add",
        "Run Ping",
        "Read Sensors",
        "Queue",
        "Rollback",
        "Generate",
        "Assign",
        "Apply",
    };
    return module >= 1 && module <= 8 ? actions[module] : "Open";
}

static const char *ModuleRowText(int module, int row) {
    static const char *rows[9][4] = {
        { "", "", "", "" },
        { "", "", "", "" },
        {
            "192.168.1.1 | Core Router | reachable | 2 ms | 0% loss",
            "192.168.1.10 | Office AP 1 | reachable | 6 ms | 0% loss",
            "192.168.1.20 | NAS Backup | reachable | 1 ms | 0% loss",
            "192.168.1.30 | UPS Rack | reachable | 4 ms | 0% loss",
        },
        {
            "Rack A temperature | 24 C | normal",
            "Rack A humidity | 42% | normal",
            "UPS battery load | 66% | normal",
            "Switch fan speed | 3100 rpm | normal",
        },
        {
            "#1042 | Office AP 1 | maintenance | assigned",
            "#1043 | Printer VLAN | open | priority medium",
            "#1044 | NAS Backup | pending review | priority low",
            "#1045 | Core Router | closed | priority high",
        },
        {
            "2026-05-28 09:15 | Core Router | ACL updated",
            "2026-05-28 10:30 | Access Switch A | VLAN 20 added",
            "2026-05-28 11:40 | Office AP 1 | channel changed",
            "2026-05-28 15:10 | UPS Rack | SNMP threshold updated",
        },
        {
            "estado_rede_05_2026.txt | monthly state | ready",
            "incidentes_abertos.txt | open incidents | ready",
            "equipamentos_falha.txt | failed devices | ready",
            "monitorizacao_diaria.txt | daily monitoring | ready",
        },
        {
            "Ana Silva | networking | available",
            "Bruno Costa | infrastructure | assigned",
            "Carla Gomes | helpdesk | available",
            "Diogo Martins | security | standby",
        },
        {
            "HTTP mode | local/server | enabled",
            "Storage path | equipamentos.dat | enabled",
            "Theme | operator light | enabled",
            "Autosave | native/tui | enabled",
        },
    };
    if (module < 2 || module > 8 || row < 0 || row >= 4) {
        return "";
    }
    return rows[module][row];
}

static const char *ModuleDetailText(int module) {
    static const char *details[] = {
        "",
        "",
        "Run host checks, review ping results, and save the monitoring log.",
        "Track rack sensor values and flag readings outside the expected range.",
        "Manage incident ordering, assignment, and pending technical work.",
        "Review configuration changes and choose rollback points from the stack.",
        "Generate network state reports and export filtered operational lists.",
        "Keep technician records available for assignment and incident ownership.",
        "Switch local/server modes and keep platform-specific runtime settings.",
    };
    return module >= 2 && module <= 8 ? details[module] : "";
}

static void PlaceholderModule(int module) {
    CLAY(CLAY_IDI("PlaceholderModule", (uint32_t)module), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = 12,
        },
    }) {
        CLAY(CLAY_IDI("ModuleMetrics", (uint32_t)module), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Module", ModuleName(module));
            Metric("Storage", ModuleFileName(module));
            Metric("Status", "Ready");
        }
        CLAY(CLAY_IDI("PlaceholderActions", (uint32_t)module), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
            },
        }) {
            Button(200u + (uint32_t)module * 5u, ModulePrimaryAction(module), PROGTP_UI_MODULE_BASE + (uintptr_t)module, true, false);
            Button(201u + (uint32_t)module * 5u, "Add", PROGTP_UI_MODULE_BASE + (uintptr_t)module, false, false);
            Button(202u + (uint32_t)module * 5u, "Update", PROGTP_UI_MODULE_BASE + (uintptr_t)module, false, false);
            Button(203u + (uint32_t)module * 5u, "Save", PROGTP_APP_ACTION_SAVE, false, false);
            Button(204u + (uint32_t)module * 5u, "Load", PROGTP_APP_ACTION_LOAD, false, false);
        }
        CLAY(CLAY_IDI("ModuleContent", (uint32_t)module), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            CLAY(CLAY_IDI("ModuleTable", (uint32_t)module), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(.min = 220) },
                    .childGap = 2,
                },
                .backgroundColor = COLOR_SURFACE,
                .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
                .cornerRadius = CLAY_CORNER_RADIUS(5),
            }) {
                CLAY(CLAY_IDI("ModuleTableHeader", (uint32_t)module), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34) },
                        .padding = { 10, 10, 0, 0 },
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = COLOR_SURFACE_DARK,
                }) {
                    TextLine(ModuleTableTitle(module), 13, COLOR_TEXT);
                }
                for (int row = 0; row < 4; ++row) {
                    CLAY(CLAY_IDI("ModuleRow", (uint32_t)module * 16u + (uint32_t)row), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34) },
                            .padding = { 10, 10, 0, 0 },
                            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = row == 0 ? COLOR_ACCENT : (row % 2 == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
                    }) {
                        AttachInteraction(PROGTP_UI_MODULE_BASE + (uintptr_t)module);
                        TextLine(ModuleRowText(module, row), 12, row == 0 ? COLOR_WHITE : COLOR_TEXT);
                    }
                }
            }
            if (!progtp_ui_compact) {
                CLAY(CLAY_IDI("ModuleDetail", (uint32_t)module), {
                    .layout = {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(14),
                        .childGap = 10,
                    },
                    .backgroundColor = COLOR_SURFACE,
                    .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                }) {
                    TextLine("Selected record", 16, COLOR_TEXT);
                    TextLine(ModuleDetailText(module), 13, COLOR_MUTED);
                    TextLine(ModuleFileName(module), 13, COLOR_ACCENT_DARK);
                }
            }
        }
    }
}

static void FormFieldRow(ProgTP_AppState *state, uint32_t id, ProgTP_AppFormField left, ProgTP_AppFormField right) {
    CLAY(CLAY_IDI("FormFieldRow", id), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .childGap = 10,
        },
    }) {
        FormTextField(id * 2u, state, left);
        FormTextField(id * 2u + 1u, state, right);
    }
}

static void EquipmentFormModal(ProgTP_AppState *state) {
    const char *title = state->modal == PROGTP_APP_MODAL_ADD_EQUIPMENT ? "Add equipment" : "Update equipment";
    CLAY(CLAY_ID("EquipmentFormModal"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(.min = 320.0f, .max = 600.0f), CLAY_SIZING_FIT(0) },
            .padding = CLAY_PADDING_ALL(18),
            .childGap = 14,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(8),
    }) {
        CLAY(CLAY_ID("EquipmentFormHeader"), {
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 4,
            },
        }) {
            TextLine(title, 21, COLOR_TEXT);
            TextLine(state->status, 13, COLOR_MUTED);
        }
        if (progtp_ui_compact) {
            CLAY(CLAY_ID("CompactFormNavigation"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 8,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
            }) {
                Button(304, "Prev Field", PROGTP_APP_ACTION_FORM_PREVIOUS_FIELD, false, false);
                CLAY(CLAY_ID("CompactFormFieldName"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight()) },
                        .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = COLOR_SURFACE_ALT,
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                }) {
                    TextLine(FormFieldLabel(state->form_field), 13, COLOR_TEXT);
                }
                Button(305, "Next Field", PROGTP_APP_ACTION_FORM_NEXT_FIELD, false, false);
            }
            FormTextField(8, state, state->form_field);
        } else {
            FormFieldRow(state, 1, PROGTP_APP_FORM_NAME, PROGTP_APP_FORM_TYPE);
            FormFieldRow(state, 2, PROGTP_APP_FORM_BRAND, PROGTP_APP_FORM_MODEL);
            FormFieldRow(state, 3, PROGTP_APP_FORM_IP, PROGTP_APP_FORM_MAC);
            FormTextField(8, state, PROGTP_APP_FORM_LOCATION);
        }
        CLAY(CLAY_ID("EquipmentStateRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            TextLine("State", 12, COLOR_MUTED);
            if (progtp_ui_compact) {
                Button(300, "Prev", PROGTP_APP_ACTION_FORM_STATE_PREVIOUS, false, false);
                CLAY(CLAY_ID("CompactFormStateValue"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(.min = 120.0f), CLAY_SIZING_FIXED(ControlHeight()) },
                        .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
                    },
                    .backgroundColor = COLOR_WHITE,
                    .border = {
                        .color = COLOR_ACCENT,
                        .width = {
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            state->terminal_rendering ? 0 : 1,
                            0,
                        },
                    },
                    .cornerRadius = CLAY_CORNER_RADIUS(5),
                }) {
                    TextLine(ProgTP_EquipmentStateName(state->form_state), 13, COLOR_TEXT);
                }
                Button(301, "Next", PROGTP_APP_ACTION_FORM_STATE_NEXT, false, false);
            } else {
                StateButton(300, state, PROGTP_EQUIPMENT_OPERATIONAL);
                StateButton(301, state, PROGTP_EQUIPMENT_FAILED);
                StateButton(302, state, PROGTP_EQUIPMENT_MAINTENANCE);
                StateButton(303, state, PROGTP_EQUIPMENT_DISABLED);
            }
        }
        CLAY(CLAY_ID("EquipmentPendingRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            TextLine("Incidents", 12, COLOR_MUTED);
            Button(
                306,
                state->form_pending ? "Pending: Yes" : "Pending: No",
                PROGTP_APP_ACTION_FORM_TOGGLE_PENDING,
                state->form_pending,
                false);
        }
        CLAY(CLAY_ID("EquipmentFormActions"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
                .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            Button(310, "Cancel", PROGTP_APP_ACTION_FORM_CANCEL, false, false);
            Button(311, "Save", PROGTP_APP_ACTION_FORM_SUBMIT, true, false);
        }
    }
}

static void RemoveConfirmModal(ProgTP_AppState *state) {
    CLAY(CLAY_ID("RemoveConfirmModal"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(.min = 300.0f, .max = 480.0f), CLAY_SIZING_FIT(0) },
            .padding = CLAY_PADDING_ALL(18),
            .childGap = 14,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(8),
    }) {
        TextLine("Remove equipment", 21, COLOR_TEXT);
        TextLine(state->modal_message, 14, COLOR_MUTED);
        if (state->status[0] != '\0') {
            TextLine(state->status, 13, COLOR_DANGER);
        }
        CLAY(CLAY_ID("RemoveActions"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
                .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            Button(320, "Cancel", PROGTP_APP_ACTION_FORM_CANCEL, false, false);
            Button(321, "Remove", PROGTP_APP_ACTION_FORM_SUBMIT, false, true);
        }
    }
}

static void ModalOverlay(ProgTP_AppState *state, Clay_Dimensions layout_dimensions) {
    if (state->modal == PROGTP_APP_MODAL_NONE) {
        return;
    }
    CLAY(CLAY_ID("ModalOverlay"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(layout_dimensions.width), CLAY_SIZING_FIXED(layout_dimensions.height) },
            .padding = CLAY_PADDING_ALL(16),
            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
        },
        .floating = {
            .zIndex = 100,
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
        .backgroundColor = COLOR_OVERLAY,
    }) {
        if (state->modal == PROGTP_APP_MODAL_REMOVE_EQUIPMENT) {
            RemoveConfirmModal(state);
        } else {
            EquipmentFormModal(state);
        }
    }
}

static void MainModule(ProgTP_AppState *state) {
    if (state->active_module == 1) {
        InventoryModule(state);
    } else if (state->active_module == 2) {
        ConnectivityModule(state);
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
