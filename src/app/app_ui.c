#include "app.h"
#include "app_internal.h"
#include "progtp_text.h"

#include <stdio.h>
#include <string.h>

void TextLine(const char *text, uint16_t size, Clay_Color color) {
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
    if (value >= PROGTP_UI_FILES_SELECT_BASE) {
        size_t idx = (size_t)(value - PROGTP_UI_FILES_SELECT_BASE);
        if (idx < ProgTP_AppGetFilesCount()) {
            progtp_interaction_state->files_selected_index = idx;
            progtp_interaction_state->files_needs_refresh = true;
        }
        return;
    }
    if (value >= PROGTP_UI_CONFIG_SELECT_BASE) {
        uint32_t entry_id = (uint32_t)(value - PROGTP_UI_CONFIG_SELECT_BASE);
        for (size_t i = 0; i < progtp_interaction_state->config_history.length; ++i) {
            if (progtp_interaction_state->config_history.items[i].id == entry_id) {
                progtp_interaction_state->selected_config_index = i;
                break;
            }
        }
        return;
    }
    if (value >= PROGTP_UI_INCIDENT_FORM_FIELD_BASE) {
        ProgTP_AppIncidentFormField field = (ProgTP_AppIncidentFormField)(value - PROGTP_UI_INCIDENT_FORM_FIELD_BASE);
        if (field >= 0 && field < PROGTP_APP_INCIDENT_FORM_FIELD_COUNT) {
            progtp_interaction_state->incident_form_field = field;
        }
        return;
    }
    if (value >= PROGTP_UI_INCIDENT_SELECT_BASE) {
        uint32_t incident_number = (uint32_t)(value - PROGTP_UI_INCIDENT_SELECT_BASE);
        for (size_t i = 0; i < progtp_interaction_state->incidents.length; ++i) {
            if (progtp_interaction_state->incidents.items[i].number == incident_number) {
                progtp_interaction_state->selected_incident_index = i;
                break;
            }
        }
        return;
    }
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
    if (value >= PROGTP_UI_SENSOR_SELECT_BASE) {
        size_t index = (size_t)(value - PROGTP_UI_SENSOR_SELECT_BASE);
        if (index < progtp_interaction_state->sensors.length) {
            progtp_interaction_state->selected_sensor_index = index;
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

void AttachInteraction(uintptr_t interaction) {
    Clay_OnHover(HandleUiInteraction, (void *)interaction); /* NOLINT(performance-no-int-to-ptr): Clay stores small integer interaction ids as opaque user data. */
}

void Button(uint32_t id, const char *label, uintptr_t interaction, bool active, bool danger) {
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

void FormTextField(uint32_t id, ProgTP_AppState *state, ProgTP_AppFormField field) {
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

void StateButton(uint32_t id, ProgTP_AppState *state, ProgTP_EquipmentState equipment_state) {
    Button(id, ProgTP_EquipmentStateName(equipment_state), PROGTP_UI_FORM_STATE_BASE + (uintptr_t)equipment_state, state->form_state == equipment_state, false);
}

void ModuleButton(ProgTP_AppState *state, int module) {
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

void Metric(const char *label, const char *value) {
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
