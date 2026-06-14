#include "app.h"
#include "app_internal.h"
#include "progtp_text.h"

#include <stdio.h>
#include <string.h>

static ProgTP_Equipment *SelectedEquipment(ProgTP_AppState *state) {
    return ProgTP_EquipmentInventoryFindByCode(&state->inventory, state->selected_code);
}

void QueueConnectivityRequest(ProgTP_AppState *state, ProgTP_ConnectivityOperation operation) {
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

void PrepareConnectivityOutputLines(ProgTP_AppState *state) {
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

void PrepareConnectivityText(ProgTP_AppState *state) {
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

    size_t count = ProgTP_EquipmentInventoryGetCount(&state->inventory);
    if (count == 0) {
        state->connectivity_row_offset = 0;
    } else if (state->connectivity_row_offset >= count) {
        state->connectivity_row_offset = ((count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }
    size_t selected_index = (size_t)-1;
    for (size_t i = 0; i < count; ++i) {
        if (ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i)->code == state->selected_code) {
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
        const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i);
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

void ConnectivityEquipmentRows(ProgTP_AppState *state) {
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

void ConnectivityResultPanel(ProgTP_AppState *state) {
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

void ConnectivityModule(ProgTP_AppState *state) {
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
