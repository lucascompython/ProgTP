#include "app.h"
#include "app_internal.h"
#include "progtp_text.h"
#include <stdio.h>
#include <string.h>

void EnsureSensorSelection(ProgTP_AppState *state) {
    if (state->sensors.length == 0) {
        state->selected_sensor_index = 0;
    } else if (state->selected_sensor_index >= state->sensors.length) {
        state->selected_sensor_index = state->sensors.length - 1u;
    }
}

bool SensorVisible(const ProgTP_AppState *state, const ProgTP_SensorReading *reading) {
    return !state->sensor_filter_anomalous || ProgTP_SensorReadingIsAnomalous(reading);
}

size_t CountVisibleSensors(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < state->sensors.length; ++i) {
        if (SensorVisible(state, &state->sensors.items[i])) {
            ++count;
        }
    }
    return count;
}

bool SelectVisibleSensorAt(ProgTP_AppState *state, size_t visible_index) {
    size_t current = 0;
    for (size_t i = 0; i < state->sensors.length; ++i) {
        if (!SensorVisible(state, &state->sensors.items[i])) {
            continue;
        }
        if (current == visible_index) {
            state->selected_sensor_index = i;
            return true;
        }
        ++current;
    }
    return false;
}

void EnsureSensorSelectionInFilter(ProgTP_AppState *state) {
    EnsureSensorSelection(state);
    if (state->sensors.length == 0 || SensorVisible(state, &state->sensors.items[state->selected_sensor_index])) {
        return;
    }
    SelectVisibleSensorAt(state, 0);
}

void MoveSensorSelection(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisibleSensors(state);
    if (visible_count == 0) {
        EnsureSensorSelection(state);
        return;
    }
    size_t current = 0;
    bool found = false;
    for (size_t i = 0; i < state->sensors.length; ++i) {
        if (!SensorVisible(state, &state->sensors.items[i])) {
            continue;
        }
        if (i == state->selected_sensor_index) {
            found = true;
            break;
        }
        ++current;
    }
    if (!found) {
        current = 0;
    }
    size_t next = current;
    if (direction > 0) {
        next = (current + 1u) % visible_count;
    } else {
        next = current == 0 ? visible_count - 1u : current - 1u;
    }
    SelectVisibleSensorAt(state, next);
}

void PageSensors(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisibleSensors(state);
    if (visible_count <= PROGTP_VISIBLE_ROWS) {
        state->sensor_row_offset = 0;
        return;
    }
    if (direction > 0) {
        size_t next = state->sensor_row_offset + PROGTP_VISIBLE_ROWS;
        state->sensor_row_offset = next < visible_count ? next : 0;
    } else if (state->sensor_row_offset == 0) {
        state->sensor_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    } else {
        state->sensor_row_offset = state->sensor_row_offset > PROGTP_VISIBLE_ROWS
            ? state->sensor_row_offset - PROGTP_VISIBLE_ROWS
            : 0;
    }
    SelectVisibleSensorAt(state, state->sensor_row_offset);
}

void QueueSensorImportRequest(ProgTP_AppState *state) {
    if (state->sensor_import_request_pending || state->sensor_import_request_in_flight) {
        SetStatus(state, "A sensor import is already running");
        return;
    }
    state->sensor_import_request_pending = true;
    SetStatus(state, "Sensor import queued");
}

void PrepareSensorText(ProgTP_AppState *state) {
    uint32_t anomaly_count = 0;
    for (size_t i = 0; i < state->sensors.length; ++i) {
        if (ProgTP_SensorReadingIsAnomalous(&state->sensors.items[i])) {
            ++anomaly_count;
        }
    }
    EnsureSensorSelectionInFilter(state);
    snprintf(state->sensor_metric_total_text, sizeof(state->sensor_metric_total_text), "%zu readings", state->sensors.length);
    snprintf(state->sensor_metric_anomaly_text, sizeof(state->sensor_metric_anomaly_text), "%u anomalies", anomaly_count);
    if (state->sensors.length > 0) {
        const ProgTP_SensorReading *selected = &state->sensors.items[state->selected_sensor_index];
        snprintf(state->sensor_metric_selected_text, sizeof(state->sensor_metric_selected_text), "%s", selected->code);
        ProgTP_SensorReadingFormatLine(selected, state->sensor_selected_text, sizeof(state->sensor_selected_text));
    } else {
        snprintf(state->sensor_metric_selected_text, sizeof(state->sensor_metric_selected_text), "%s", "None");
        snprintf(state->sensor_selected_text, sizeof(state->sensor_selected_text), "%s", "No sensor readings imported");
    }
    snprintf(
        state->sensor_search_display,
        sizeof(state->sensor_search_display),
        "%s%s",
        state->sensor_search_text[0] != '\0' ? state->sensor_search_text : "Sensor code",
        state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE ? "_" : "");

    state->sensor_row_count = 0;
    size_t visible_count = CountVisibleSensors(state);
    if (visible_count == 0) {
        state->sensor_row_offset = 0;
        snprintf(state->sensor_row_page_text, sizeof(state->sensor_row_page_text), "%s", "No sensor readings in this view");
        return;
    }
    if (state->sensor_row_offset >= visible_count) {
        state->sensor_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }
    size_t selected_visible_index = (size_t)-1;
    size_t visible_index = 0;
    for (size_t i = 0; i < state->sensors.length; ++i) {
        if (!SensorVisible(state, &state->sensors.items[i])) {
            continue;
        }
        if (i == state->selected_sensor_index) {
            selected_visible_index = visible_index;
            break;
        }
        ++visible_index;
    }
    if (selected_visible_index != (size_t)-1 &&
        (selected_visible_index < state->sensor_row_offset ||
         selected_visible_index >= state->sensor_row_offset + PROGTP_VISIBLE_ROWS)) {
        state->sensor_row_offset = (selected_visible_index / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }

    visible_index = 0;
    for (size_t i = 0; i < state->sensors.length && state->sensor_row_count < PROGTP_VISIBLE_ROWS; ++i) {
        const ProgTP_SensorReading *reading = &state->sensors.items[i];
        if (!SensorVisible(state, reading)) {
            continue;
        }
        if (visible_index >= state->sensor_row_offset) {
            size_t row = state->sensor_row_count++;
            state->sensor_row_indices[row] = i;
            snprintf(
                state->sensor_row_texts[row],
                sizeof(state->sensor_row_texts[row]),
                "%s | %s | %.2f %s | %s",
                reading->code,
                reading->type,
                reading->value,
                reading->unit,
                reading->state);
        }
        ++visible_index;
    }
    snprintf(
        state->sensor_row_page_text,
        sizeof(state->sensor_row_page_text),
        "Showing %zu-%zu of %zu",
        state->sensor_row_offset + 1u,
        state->sensor_row_offset + state->sensor_row_count,
        visible_count);
}

void SensorRows(ProgTP_AppState *state) {
    CLAY(CLAY_ID("SensorTable"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_GROW(.min = 380.0f),
                CLAY_SIZING_GROW(.min = 220.0f),
            },
            .childGap = 2,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        CLAY(CLAY_ID("SensorTableHeader"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight() + 8.0f) },
                .padding = { 10, 10, 4, 4 },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = COLOR_SURFACE_DARK,
        }) {
            CLAY(CLAY_ID("SensorTableTitle"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 2,
                },
            }) {
                TextLine("Latest sensor readings", 13, COLOR_TEXT);
                TextLine(state->sensor_row_page_text, 12, COLOR_MUTED);
            }
            Button(500, "Prev Page", PROGTP_APP_ACTION_SENSOR_PAGE_PREVIOUS, false, false);
            Button(501, "Next Page", PROGTP_APP_ACTION_SENSOR_PAGE_NEXT, false, false);
        }
        for (size_t i = 0; i < state->sensor_row_count; ++i) {
            size_t reading_index = state->sensor_row_indices[i];
            const ProgTP_SensorReading *reading = &state->sensors.items[reading_index];
            bool selected = reading_index == state->selected_sensor_index;
            bool anomalous = ProgTP_SensorReadingIsAnomalous(reading);
            CLAY(CLAY_IDI("SensorRow", (uint32_t)i), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = selected ? COLOR_ACCENT : anomalous ? (Clay_Color){255, 238, 218, 255} : (i % 2u == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
            }) {
                AttachInteraction(PROGTP_UI_SENSOR_SELECT_BASE + (uintptr_t)reading_index);
                TextLine(state->sensor_row_texts[i], 12, selected ? COLOR_WHITE : anomalous ? COLOR_DANGER : COLOR_TEXT);
            }
        }
    }
}

void SensorDetailPanel(ProgTP_AppState *state) {
    bool anomalous = state->sensors.length > 0 &&
        ProgTP_SensorReadingIsAnomalous(&state->sensors.items[state->selected_sensor_index]);
    CLAY(CLAY_ID("SensorDetailPanel"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(360.0f),
                CLAY_SIZING_GROW(.min = 180.0f),
            },
            .padding = CLAY_PADDING_ALL(14),
            .childGap = 8,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        TextLine("Selected reading", 16, COLOR_TEXT);
        TextLine(state->sensor_selected_text, 13, anomalous ? COLOR_DANGER : COLOR_MUTED);
        TextLine("sensores_rack.txt", 12, COLOR_ACCENT_DARK);
        TextLine("leituras_sensores.dat / log_sensores.txt", 12, COLOR_MUTED);
        if (state->sensor_has_import_result) {
            TextLine(state->sensor_import_result.summary, 13, COLOR_TEXT);
        }
        if (state->status[0] != '\0') {
            TextLine(state->status, 13, anomalous ? COLOR_DANGER : COLOR_MUTED);
        }
    }
}

void SensorModule(ProgTP_AppState *state) {
    CLAY(CLAY_ID("SensorModule"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = progtp_ui_compact ? 8 : 12,
        },
    }) {
        CLAY(CLAY_ID("SensorMetrics"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Readings", state->sensor_metric_total_text);
            Metric("Anomalies", state->sensor_metric_anomaly_text);
            Metric("Selected", state->sensor_metric_selected_text);
        }
        CLAY(CLAY_ID("SensorActions"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(510, "Import", PROGTP_APP_ACTION_SENSOR_IMPORT, true, false);
            Button(511, "Prev", PROGTP_APP_ACTION_SENSOR_PREVIOUS, false, false);
            Button(512, "Next", PROGTP_APP_ACTION_SENSOR_NEXT, false, false);
            Button(513, "All", PROGTP_APP_ACTION_SENSOR_FILTER_ALL, !state->sensor_filter_anomalous, false);
            Button(514, "Anomalies", PROGTP_APP_ACTION_SENSOR_FILTER_ANOMALOUS, state->sensor_filter_anomalous, false);
        }
        CLAY(CLAY_ID("SensorFilePath"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            Button(516, "File", PROGTP_APP_ACTION_SENSOR_CHOOSE_FILE, false, false);
            Button(517, "Fetch API", PROGTP_APP_ACTION_SENSOR_FETCH_API, !state->sensor_api_fetch_request_pending && !state->sensor_api_fetch_in_flight, false);
            TextLine(state->sensor_input_path, 12, COLOR_MUTED);
        }
        CLAY(CLAY_ID("SensorSearch"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            CLAY(CLAY_ID("SensorSearchInput"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(.min = 180.0f), CLAY_SIZING_FIXED(ControlHeight()) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = COLOR_SURFACE,
                .border = {
                    .color = state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE ? COLOR_ACCENT : COLOR_LINE,
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
                AttachInteraction(PROGTP_APP_ACTION_SENSOR_SEARCH_FIELD);
                TextLine(state->sensor_search_display, 13, state->input_mode == PROGTP_APP_INPUT_SENSOR_CODE ? COLOR_TEXT : COLOR_MUTED);
            }
            Button(515, "Find", PROGTP_APP_ACTION_INPUT_SUBMIT, false, false);
        }
        CLAY(CLAY_ID("SensorContent"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            SensorRows(state);
            SensorDetailPanel(state);
        }
    }
}

void SensorFileModal(ProgTP_AppState *state) {
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
