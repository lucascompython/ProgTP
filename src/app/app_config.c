#include "app.h"
#include "app_internal.h"
#include "progtp_text.h"

#include <stdio.h>
#include <string.h>

static bool ConfigVisible(const ProgTP_AppState *state, const ProgTP_ConfigEntry *entry) {
    if (state->config_filter_state == 0) {
        return true;
    }
    return (uint32_t)entry->entry_state + 1u == state->config_filter_state;
}

static size_t CountVisibleConfig(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < state->config_history.length; ++i) {
        if (ConfigVisible(state, &state->config_history.items[i])) {
            ++count;
        }
    }
    return count;
}

static bool SelectVisibleConfigAt(ProgTP_AppState *state, size_t visible_index) {
    size_t current = 0;
    for (size_t i = 0; i < state->config_history.length; ++i) {
        if (!ConfigVisible(state, &state->config_history.items[i])) {
            continue;
        }
        if (current == visible_index) {
            state->selected_config_index = i;
            return true;
        }
        ++current;
    }
    return false;
}

static void EnsureConfigSelection(ProgTP_AppState *state) {
    if (state->config_history.length == 0) {
        state->selected_config_index = 0;
        return;
    }
    if (state->selected_config_index >= state->config_history.length) {
        state->selected_config_index = state->config_history.length - 1u;
    }
}

void MoveConfigSelection(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisibleConfig(state);
    if (visible_count == 0) {
        EnsureConfigSelection(state);
        return;
    }
    size_t current = 0;
    bool found = false;
    for (size_t i = 0; i < state->config_history.length; ++i) {
        if (!ConfigVisible(state, &state->config_history.items[i])) {
            continue;
        }
        if (i == state->selected_config_index) {
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
    SelectVisibleConfigAt(state, next);
}

void PageConfig(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisibleConfig(state);
    if (visible_count <= PROGTP_VISIBLE_ROWS) {
        state->config_row_offset = 0;
        return;
    }
    if (direction > 0) {
        size_t next = state->config_row_offset + PROGTP_VISIBLE_ROWS;
        state->config_row_offset = next < visible_count ? next : 0;
    } else if (state->config_row_offset == 0) {
        state->config_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    } else {
        state->config_row_offset = state->config_row_offset > PROGTP_VISIBLE_ROWS
            ? state->config_row_offset - PROGTP_VISIBLE_ROWS
            : 0;
    }
    SelectVisibleConfigAt(state, state->config_row_offset);
}

void PrepareConfigText(ProgTP_AppState *state) {
    size_t applied = ProgTP_ConfigHistoryAppliedCount(&state->config_history);
    size_t undone = ProgTP_ConfigHistoryUndoneCount(&state->config_history);
    snprintf(state->config_metric_total_text, sizeof(state->config_metric_total_text), "%zu total", state->config_history.length);
    snprintf(state->config_metric_applied_text, sizeof(state->config_metric_applied_text), "%zu applied", applied);
    snprintf(state->config_metric_undone_text, sizeof(state->config_metric_undone_text), "%zu undone", undone);

    EnsureConfigSelection(state);

    if (state->config_history.length > 0 && state->selected_config_index < state->config_history.length) {
        const ProgTP_ConfigEntry *selected = &state->config_history.items[state->selected_config_index];
        ProgTP_ConfigHistoryFormatDetail(selected, state->config_selected_text, sizeof(state->config_selected_text));
    } else {
        snprintf(state->config_selected_text, sizeof(state->config_selected_text), "No configuration entry selected");
    }

    state->config_row_count = 0;
    size_t visible_count = CountVisibleConfig(state);
    if (visible_count == 0) {
        state->config_row_offset = 0;
        snprintf(state->config_row_page_text, sizeof(state->config_row_page_text), "%s", "No configuration entries");
        return;
    }
    if (state->config_row_offset >= visible_count) {
        state->config_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }

    size_t selected_visible_index = (size_t)-1;
    size_t visible_index = 0;
    for (size_t i = 0; i < state->config_history.length; ++i) {
        if (!ConfigVisible(state, &state->config_history.items[i])) {
            continue;
        }
        if (i == state->selected_config_index) {
            selected_visible_index = visible_index;
            break;
        }
        ++visible_index;
    }
    if (selected_visible_index != (size_t)-1 &&
        (selected_visible_index < state->config_row_offset ||
         selected_visible_index >= state->config_row_offset + PROGTP_VISIBLE_ROWS)) {
        state->config_row_offset = (selected_visible_index / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }

    visible_index = 0;
    for (size_t i = 0; i < state->config_history.length && state->config_row_count < PROGTP_VISIBLE_ROWS; ++i) {
        const ProgTP_ConfigEntry *entry = &state->config_history.items[i];
        if (!ConfigVisible(state, entry)) {
            continue;
        }
        if (visible_index >= state->config_row_offset) {
            size_t row = state->config_row_count++;
            ProgTP_ConfigHistoryFormatRow(entry, state->config_row_texts[row], sizeof(state->config_row_texts[row]));
        }
        ++visible_index;
    }
    snprintf(
        state->config_row_page_text,
        sizeof(state->config_row_page_text),
        "Showing %zu-%zu of %zu",
        state->config_row_offset + 1u,
        state->config_row_offset + state->config_row_count,
        visible_count);
}

static void ConfigRows(ProgTP_AppState *state) {
    CLAY(CLAY_ID("ConfigTable"), {
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
        CLAY(CLAY_ID("ConfigTableHeader"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight() + 8.0f) },
                .padding = { 10, 10, 4, 4 },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = COLOR_SURFACE_DARK,
        }) {
            CLAY(CLAY_ID("ConfigTableTitle"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 2,
                },
            }) {
                TextLine("Configuration stack", 13, COLOR_TEXT);
                TextLine(state->config_row_page_text, 12, COLOR_MUTED);
            }
            Button(800, "Prev Page", PROGTP_APP_ACTION_CONFIG_PAGE_PREVIOUS, false, false);
            Button(801, "Next Page", PROGTP_APP_ACTION_CONFIG_PAGE_NEXT, false, false);
        }
        for (size_t i = 0; i < state->config_row_count; ++i) {
            size_t entry_index = (size_t)-1;
            size_t visible_index = 0;
            for (size_t j = 0; j < state->config_history.length; ++j) {
                if (!ConfigVisible(state, &state->config_history.items[j])) {
                    continue;
                }
                if (visible_index == state->config_row_offset + i) {
                    entry_index = j;
                    break;
                }
                ++visible_index;
            }
            if (entry_index == (size_t)-1) {
                continue;
            }
            bool selected = entry_index == state->selected_config_index;
            CLAY(CLAY_IDI("ConfigRow", (uint32_t)i), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = selected ? COLOR_ACCENT : (i % 2u == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
            }) {
                AttachInteraction(PROGTP_UI_CONFIG_SELECT_BASE + (uintptr_t)state->config_history.items[entry_index].id);
                TextLine(state->config_row_texts[i], 12, selected ? COLOR_WHITE : COLOR_TEXT);
            }
        }
    }
}

static void ConfigDetailPanel(ProgTP_AppState *state) {
    CLAY(CLAY_ID("ConfigDetailPanel"), {
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
        TextLine("Selected change", 16, COLOR_TEXT);
        if (state->config_history.length > 0 && state->selected_config_index < state->config_history.length) {
            TextLine(state->config_selected_text, 12, COLOR_TEXT);
        } else {
            TextLine("No configuration entry selected", 13, COLOR_MUTED);
        }
        TextLine("configuracoes.dat", 12, COLOR_ACCENT_DARK);
        if (state->status[0] != '\0') {
            TextLine(state->status, 13, COLOR_MUTED);
        }
    }
}

void ConfigModule(ProgTP_AppState *state) {
    CLAY(CLAY_ID("ConfigModule"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = progtp_ui_compact ? 8 : 12,
        },
    }) {
        CLAY(CLAY_ID("ConfigMetrics"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Total", state->config_metric_total_text);
            Metric("Applied", state->config_metric_applied_text);
            Metric("Undone", state->config_metric_undone_text);
        }
        CLAY(CLAY_ID("ConfigActions"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(810, "Undo", PROGTP_APP_ACTION_CONFIG_UNDO, ProgTP_ConfigHistoryCanUndo(&state->config_history), false);
            Button(811, "Redo", PROGTP_APP_ACTION_CONFIG_REDO, ProgTP_ConfigHistoryCanRedo(&state->config_history), false);
            Button(812, "Import", PROGTP_APP_ACTION_CONFIG_IMPORT, true, false);
            Button(813, "Delete", PROGTP_APP_ACTION_CONFIG_DELETE, state->config_history.length > 0, true);
        }
        CLAY(CLAY_ID("ConfigFilters"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(820, "All", PROGTP_APP_ACTION_CONFIG_FILTER_ALL, state->config_filter_state == 0, false);
            Button(821, "Undone", PROGTP_APP_ACTION_CONFIG_FILTER_UNDONE, state->config_filter_state == 2, false);
            Button(822, "Prev", PROGTP_APP_ACTION_CONFIG_PREVIOUS, state->config_history.length > 1, false);
            Button(823, "Next", PROGTP_APP_ACTION_CONFIG_NEXT, state->config_history.length > 1, false);
        }
        CLAY(CLAY_ID("ConfigContent"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            ConfigRows(state);
            ConfigDetailPanel(state);
        }
    }
}
