#include "app.h"
#include "app_internal.h"

#include <stdio.h>
#include <string.h>

static const char *IncidentStateLabel(ProgTP_IncidentState state) {
    switch (state) {
        case PROGTP_INCIDENT_PENDING: return "Pending";
        case PROGTP_INCIDENT_IN_PROGRESS: return "In Progress";
        case PROGTP_INCIDENT_COMPLETED: return "Completed";
    }
    return "Unknown";
}

static void AddIncidentDetailLine(ProgTP_AppState *state, const char *label, const char *value) {
    size_t capacity = sizeof(state->incident_detail_texts) / sizeof(state->incident_detail_texts[0]);
    if (state->incident_detail_count >= capacity) {
        return;
    }
    snprintf(
        state->incident_detail_texts[state->incident_detail_count],
        sizeof(state->incident_detail_texts[state->incident_detail_count]),
        "%s%s",
        label,
        value ? value : "");
    ++state->incident_detail_count;
}

static bool IncidentVisible(const ProgTP_AppState *state, const ProgTP_Incident *incident) {
    if (state->incident_filter_state == 0) {
        return true;
    }
    return (uint32_t)incident->state == state->incident_filter_state - 1u;
}

static size_t CountVisibleIncidents(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < state->incidents.length; ++i) {
        if (IncidentVisible(state, &state->incidents.items[i])) {
            ++count;
        }
    }
    return count;
}

static bool SelectVisibleIncidentAt(ProgTP_AppState *state, size_t visible_index) {
    size_t current = 0;
    for (size_t i = 0; i < state->incidents.length; ++i) {
        if (!IncidentVisible(state, &state->incidents.items[i])) {
            continue;
        }
        if (current == visible_index) {
            state->selected_incident_index = i;
            return true;
        }
        ++current;
    }
    return false;
}

static void EnsureIncidentSelection(ProgTP_AppState *state) {
    if (state->incidents.length == 0) {
        state->selected_incident_index = 0;
    } else if (state->selected_incident_index >= state->incidents.length) {
        state->selected_incident_index = state->incidents.length - 1u;
    }
}

static void EnsureIncidentSelectionInFilter(ProgTP_AppState *state) {
    EnsureIncidentSelection(state);
    if (state->incidents.length == 0 || IncidentVisible(state, &state->incidents.items[state->selected_incident_index])) {
        return;
    }
    SelectVisibleIncidentAt(state, 0);
}

void MoveIncidentSelection(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisibleIncidents(state);
    if (visible_count == 0) {
        EnsureIncidentSelection(state);
        return;
    }
    size_t current = 0;
    bool found = false;
    for (size_t i = 0; i < state->incidents.length; ++i) {
        if (!IncidentVisible(state, &state->incidents.items[i])) {
            continue;
        }
        if (i == state->selected_incident_index) {
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
    SelectVisibleIncidentAt(state, next);
}

void PageIncidents(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisibleIncidents(state);
    if (visible_count <= PROGTP_VISIBLE_ROWS) {
        state->incident_row_offset = 0;
        return;
    }
    if (direction > 0) {
        size_t next = state->incident_row_offset + PROGTP_VISIBLE_ROWS;
        state->incident_row_offset = next < visible_count ? next : 0;
    } else if (state->incident_row_offset == 0) {
        state->incident_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    } else {
        state->incident_row_offset = state->incident_row_offset > PROGTP_VISIBLE_ROWS
            ? state->incident_row_offset - PROGTP_VISIBLE_ROWS
            : 0;
    }
    SelectVisibleIncidentAt(state, state->incident_row_offset);
}

void PrepareIncidentText(ProgTP_AppState *state) {
    if (state->needs_incident_reload) {
        state->needs_incident_reload = false;
        ProgTP_IncidentStoreLoad(&state->incidents, "incidentes.dat", state->status, sizeof(state->status));
    }
    uint32_t pending_count = 0;
    uint32_t in_progress_count = 0;
    uint32_t completed_count = 0;
    for (size_t i = 0; i < state->incidents.length; ++i) {
        switch (state->incidents.items[i].state) {
            case PROGTP_INCIDENT_PENDING: ++pending_count; break;
            case PROGTP_INCIDENT_IN_PROGRESS: ++in_progress_count; break;
            case PROGTP_INCIDENT_COMPLETED: ++completed_count; break;
        }
    }
    snprintf(state->incident_metric_total_text, sizeof(state->incident_metric_total_text), "%zu total", state->incidents.length);
    snprintf(state->incident_metric_pending_text, sizeof(state->incident_metric_pending_text), "%u pending", pending_count);
    snprintf(state->incident_metric_in_progress_text, sizeof(state->incident_metric_in_progress_text), "%u in progress", in_progress_count);
    snprintf(state->incident_metric_completed_text, sizeof(state->incident_metric_completed_text), "%u completed", completed_count);

    EnsureIncidentSelectionInFilter(state);
    state->incident_detail_count = 0;
    if (state->incidents.length > 0 && state->selected_incident_index < state->incidents.length) {
        const ProgTP_Incident *selected = &state->incidents.items[state->selected_incident_index];
        snprintf(
            state->incident_selected_text,
            sizeof(state->incident_selected_text),
            "#%u | %s | %s | %s | %s",
            selected->number,
            selected->type,
            selected->priority,
            IncidentStateLabel(selected->state),
            selected->source);
        AddIncidentDetailLine(state, "Source: ", selected->source);
        AddIncidentDetailLine(state, "Description: ", selected->description);
        if (selected->technician[0] != '\0') {
            AddIncidentDetailLine(state, "Technician: ", selected->technician);
        }
        if (selected->created_at[0] != '\0') {
            AddIncidentDetailLine(state, "Created: ", selected->created_at);
        }
        if (selected->completed_at[0] != '\0') {
            AddIncidentDetailLine(state, "Completed: ", selected->completed_at);
        }
    } else {
        snprintf(state->incident_selected_text, sizeof(state->incident_selected_text), "No incident selected");
    }

    state->incident_row_count = 0;
    size_t visible_count = CountVisibleIncidents(state);
    if (visible_count == 0) {
        state->incident_row_offset = 0;
        snprintf(state->incident_row_page_text, sizeof(state->incident_row_page_text), "%s", "No incidents in this view");
        return;
    }
    if (state->incident_row_offset >= visible_count) {
        state->incident_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }
    size_t selected_visible_index = (size_t)-1;
    size_t visible_index = 0;
    for (size_t i = 0; i < state->incidents.length; ++i) {
        if (!IncidentVisible(state, &state->incidents.items[i])) {
            continue;
        }
        if (i == state->selected_incident_index) {
            selected_visible_index = visible_index;
            break;
        }
        ++visible_index;
    }
    if (selected_visible_index != (size_t)-1 &&
        (selected_visible_index < state->incident_row_offset ||
         selected_visible_index >= state->incident_row_offset + PROGTP_VISIBLE_ROWS)) {
        state->incident_row_offset = (selected_visible_index / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }

    visible_index = 0;
    for (size_t i = 0; i < state->incidents.length && state->incident_row_count < PROGTP_VISIBLE_ROWS; ++i) {
        const ProgTP_Incident *incident = &state->incidents.items[i];
        if (!IncidentVisible(state, incident)) {
            continue;
        }
        if (visible_index >= state->incident_row_offset) {
            size_t row = state->incident_row_count++;
            state->incident_row_numbers[row] = incident->number;
            snprintf(
                state->incident_row_texts[row],
                sizeof(state->incident_row_texts[row]),
                "#%u | %s | %s | %s | %s",
                incident->number,
                incident->created_at,
                incident->type,
                incident->priority,
                IncidentStateLabel(incident->state));
        }
        ++visible_index;
    }
    snprintf(
        state->incident_row_page_text,
        sizeof(state->incident_row_page_text),
        "Showing %zu-%zu of %zu",
        state->incident_row_offset + 1u,
        state->incident_row_offset + state->incident_row_count,
        visible_count);
}

void IncidentRows(ProgTP_AppState *state) {
    CLAY(CLAY_ID("IncidentTable"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_GROW(.min = 420.0f),
                CLAY_SIZING_GROW(.min = 220.0f),
            },
            .childGap = 2,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        CLAY(CLAY_ID("IncidentTableHeader"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight() + 8.0f) },
                .padding = { 10, 10, 4, 4 },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = COLOR_SURFACE_DARK,
        }) {
            CLAY(CLAY_ID("IncidentTableTitle"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 2,
                },
            }) {
                TextLine("Incident queue", 13, COLOR_TEXT);
                TextLine(state->incident_row_page_text, 12, COLOR_MUTED);
            }
            Button(600, "Prev Page", PROGTP_APP_ACTION_INCIDENT_PAGE_PREVIOUS, false, false);
            Button(601, "Next Page", PROGTP_APP_ACTION_INCIDENT_PAGE_NEXT, false, false);
        }
        for (size_t i = 0; i < state->incident_row_count; ++i) {
            uint32_t incident_number = state->incident_row_numbers[i];
            bool selected = state->incidents.length > 0 &&
                state->selected_incident_index < state->incidents.length &&
                state->incidents.items[state->selected_incident_index].number == incident_number;
            CLAY(CLAY_IDI("IncidentRow", (uint32_t)i), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = selected ? COLOR_ACCENT : (i % 2u == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
            }) {
                AttachInteraction(PROGTP_UI_INCIDENT_SELECT_BASE + (uintptr_t)incident_number);
                TextLine(state->incident_row_texts[i], 12, selected ? COLOR_WHITE : COLOR_TEXT);
            }
        }
    }
}

void IncidentDetailPanel(ProgTP_AppState *state) {
    CLAY(CLAY_ID("IncidentDetailPanel"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(380.0f),
                CLAY_SIZING_GROW(.min = 180.0f),
            },
            .padding = CLAY_PADDING_ALL(14),
            .childGap = 8,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        TextLine("Selected incident", 16, COLOR_TEXT);
        if (state->incidents.length > 0 && state->selected_incident_index < state->incidents.length) {
            TextLine(state->incident_selected_text, 13, COLOR_TEXT);
            for (size_t i = 0; i < state->incident_detail_count; ++i) {
                TextLine(state->incident_detail_texts[i], 12, COLOR_MUTED);
            }
        } else {
            TextLine("No incident selected", 13, COLOR_MUTED);
        }
        TextLine("incidentes.dat", 12, COLOR_ACCENT_DARK);
        if (state->status[0] != '\0') {
            TextLine(state->status, 13, COLOR_MUTED);
        }
    }
}

void IncidentModule(ProgTP_AppState *state) {
    CLAY(CLAY_ID("IncidentModule"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = progtp_ui_compact ? 8 : 12,
        },
    }) {
        CLAY(CLAY_ID("IncidentMetrics"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Total", state->incident_metric_total_text);
            Metric("Pending", state->incident_metric_pending_text);
            Metric("In Progress", state->incident_metric_in_progress_text);
            Metric("Completed", state->incident_metric_completed_text);
        }
        CLAY(CLAY_ID("IncidentActions"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(610, "New", PROGTP_APP_ACTION_INCIDENT_ADD, true, false);
            Button(611, "Edit", PROGTP_APP_ACTION_INCIDENT_EDIT, false, false);
            Button(612, "Delete", PROGTP_APP_ACTION_INCIDENT_DELETE, false, true);
            Button(613, "Start", PROGTP_APP_ACTION_INCIDENT_START, false, false);
            Button(614, "Complete", PROGTP_APP_ACTION_INCIDENT_COMPLETE, false, false);
            Button(615, "Auto-import", PROGTP_APP_ACTION_INCIDENT_AUTO_IMPORT, false, false);
        }
        CLAY(CLAY_ID("IncidentFilters"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(620, "All", PROGTP_APP_ACTION_INCIDENT_FILTER_ALL, state->incident_filter_state == 0, false);
            Button(621, "Pending", PROGTP_APP_ACTION_INCIDENT_FILTER_PENDING, state->incident_filter_state == 1, false);
            Button(622, "In Progress", PROGTP_APP_ACTION_INCIDENT_FILTER_IN_PROGRESS, state->incident_filter_state == 2, false);
            Button(623, "Completed", PROGTP_APP_ACTION_INCIDENT_FILTER_COMPLETED, state->incident_filter_state == 3, false);
        }
        CLAY(CLAY_ID("IncidentContent"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            IncidentRows(state);
            IncidentDetailPanel(state);
        }
    }
}
