#include "app.h"
#include "app_internal.h"

#include "progtp_text.h"

#include <stdio.h>
#include <string.h>

static ProgTP_Equipment *SelectedEquipment(ProgTP_AppState *state) {
    return ProgTP_EquipmentInventoryFindByCode(&state->inventory, state->selected_code);
}

char *FormFieldBuffer(ProgTP_AppState *state, ProgTP_AppFormField field, size_t *buffer_size) {
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

const char *FormFieldLabel(ProgTP_AppFormField field) {
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

void CloseModal(ProgTP_AppState *state) {
    state->modal = PROGTP_APP_MODAL_NONE;
    state->input_mode = PROGTP_APP_INPUT_NONE;
}

void OpenSensorFileModal(ProgTP_AppState *state) {
    state->modal = PROGTP_APP_MODAL_SENSOR_FILE;
    state->input_mode = PROGTP_APP_INPUT_NONE;
}

void SubmitSensorFile(ProgTP_AppState *state) {
    CloseModal(state);
    snprintf(state->status, sizeof(state->status), "Sensor file: %.280s", state->sensor_input_path);
}

void OpenAddModal(ProgTP_AppState *state) {
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

void OpenUpdateModal(ProgTP_AppState *state) {
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

void OpenRemoveModal(ProgTP_AppState *state) {
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

void SubmitEquipmentForm(ProgTP_AppState *state) {
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

void ConfirmRemoveSelected(ProgTP_AppState *state) {
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

void AdvanceFormField(ProgTP_AppState *state, int direction) {
    int field = (int)state->form_field + direction;
    if (field < 0) {
        field = (int)PROGTP_APP_FORM_FIELD_COUNT - 1;
    } else if (field >= (int)PROGTP_APP_FORM_FIELD_COUNT) {
        field = 0;
    }
    state->form_field = (ProgTP_AppFormField)field;
}

void BackspaceFormField(ProgTP_AppState *state) {
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
    } else if (state->modal == PROGTP_APP_MODAL_SENSOR_FILE) {
        SubmitSensorFile(state);
    }
}

bool HandleModalAction(ProgTP_AppState *state, ProgTP_AppAction action) {
    if (state->modal == PROGTP_APP_MODAL_NONE) {
        return false;
    }
    if (state->modal == PROGTP_APP_MODAL_SENSOR_FILE) {
        switch (action) {
            case PROGTP_APP_ACTION_INPUT_BACKSPACE: {
                size_t len = strlen(state->sensor_input_path);
                if (len > 0) {
                    state->sensor_input_path[len - 1u] = '\0';
                }
                return true;
            }
            case PROGTP_APP_ACTION_INPUT_SUBMIT:
            case PROGTP_APP_ACTION_FORM_SUBMIT: SubmitSensorFile(state); return true;
            case PROGTP_APP_ACTION_FORM_CANCEL: CloseModal(state); SetStatus(state, "Canceled"); return true;
            default: return true;
        }
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

static void SensorFileModal(ProgTP_AppState *state) {
    CLAY(CLAY_ID("SensorFileModal"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(.min = 320.0f, .max = 500.0f), CLAY_SIZING_FIT(0) },
            .padding = CLAY_PADDING_ALL(18),
            .childGap = 14,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(8),
    }) {
        CLAY(CLAY_ID("SensorFileHeader"), {
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 4,
            },
        }) {
            TextLine("Sensor file path", 21, COLOR_TEXT);
            TextLine(state->status, 13, COLOR_MUTED);
        }
        CLAY(CLAY_ID("SensorFileInput"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight()) },
                .padding = { 10, 10, 0, 0 },
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
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
            TextLine(state->sensor_input_path, 13, COLOR_TEXT);
        }
        TextLine("Press Enter to confirm or Esc to cancel", 12, COLOR_MUTED);
        CLAY(CLAY_ID("SensorFileActions"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 8,
                .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            Button(520, "Cancel", PROGTP_APP_ACTION_FORM_CANCEL, false, false);
            Button(521, "Save", PROGTP_APP_ACTION_FORM_SUBMIT, true, false);
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

void ModalOverlay(ProgTP_AppState *state, Clay_Dimensions layout_dimensions) {
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
        } else if (state->modal == PROGTP_APP_MODAL_SENSOR_FILE) {
            SensorFileModal(state);
        } else {
            EquipmentFormModal(state);
        }
    }
}
