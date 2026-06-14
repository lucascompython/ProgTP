#include "app.h"
#include "app_internal.h"
#include "progtp_text.h"
#include <stdio.h>
#include <string.h>

static ProgTP_Equipment *SelectedEquipment(ProgTP_AppState *state) {
    return ProgTP_EquipmentInventoryFindByCode(&state->inventory, state->selected_code);
}

static bool IsFirstTypeOccurrence(const ProgTP_AppState *state, size_t item_index) {
    const char *type = ProgTP_EquipmentInventoryGetByIndex(&state->inventory, item_index)->type;
    for (size_t i = 0; i < item_index; ++i) {
        if (ProgTP_TextEqualsIgnoreCase(ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i)->type, type)) {
            return false;
        }
    }
    return true;
}

static size_t TypeFilterCount(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < ProgTP_EquipmentInventoryGetCount(&state->inventory); ++i) {
        if (IsFirstTypeOccurrence(state, i)) {
            ++count;
        }
    }
    return count;
}

static const char *TypeFilterAt(const ProgTP_AppState *state, size_t type_index) {
    size_t current = 0;
    for (size_t i = 0; i < ProgTP_EquipmentInventoryGetCount(&state->inventory); ++i) {
        if (!IsFirstTypeOccurrence(state, i)) {
            continue;
        }
        if (current == type_index) {
            return ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i)->type;
        }
        ++current;
    }
    return NULL;
}

bool MatchesCurrentView(const ProgTP_AppState *state, const ProgTP_Equipment *equipment) {
    if (state->state_filter_enabled && equipment->state != state->state_filter) {
        return false;
    }
    if (state->type_filter[0] != '\0' && !ProgTP_TextEqualsIgnoreCase(equipment->type, state->type_filter)) {
        return false;
    }
    return true;
}

static bool SelectFilteredRowAt(ProgTP_AppState *state, size_t filtered_index) {
    size_t index = 0;
    for (size_t i = 0; i < ProgTP_EquipmentInventoryGetCount(&state->inventory); ++i) {
        ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndexMut(&state->inventory, i);
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

static size_t CountFilteredRows(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < ProgTP_EquipmentInventoryGetCount(&state->inventory); ++i) {
        if (MatchesCurrentView(state, ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i))) {
            ++count;
        }
    }
    return count;
}

void EnsureSelectionInCurrentView(ProgTP_AppState *state) {
    ProgTP_Equipment *selected = SelectedEquipment(state);
    if (selected && MatchesCurrentView(state, selected)) {
        return;
    }
    if (!SelectFilteredRowAt(state, 0)) {
        EnsureSelection(state);
    }
}

static void ResetRowNavigation(ProgTP_AppState *state) {
    state->row_offset = 0;
    EnsureSelectionInCurrentView(state);
}

void SetStateFilter(ProgTP_AppState *state, bool enabled, ProgTP_EquipmentState equipment_state) {
    state->state_filter_enabled = enabled;
    state->state_filter = equipment_state;
    ResetRowNavigation(state);
}

void CycleStateFilter(ProgTP_AppState *state, int direction) {
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

void SetTypeFilter(ProgTP_AppState *state, const char *type) {
    snprintf(state->type_filter, sizeof(state->type_filter), "%s", type ? type : "");
    ResetRowNavigation(state);
}

void CycleTypeFilter(ProgTP_AppState *state, int direction) {
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

void MoveSelection(ProgTP_AppState *state, int direction) {
    size_t filtered_count = CountFilteredRows(state);
    if (filtered_count == 0) {
        state->selected_code = 0;
        return;
    }
    size_t current_index = 0;
    bool found = false;
    for (size_t i = 0; i < ProgTP_EquipmentInventoryGetCount(&state->inventory); ++i) {
        ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndexMut(&state->inventory, i);
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

void MoveInventorySelection(ProgTP_AppState *state, int direction) {
    size_t count = ProgTP_EquipmentInventoryGetCount(&state->inventory);
    if (count == 0) {
        state->selected_code = 0;
        return;
    }
    size_t current = 0;
    for (size_t i = 0; i < count; ++i) {
        if (ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i)->code == state->selected_code) {
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
    state->selected_code = ProgTP_EquipmentInventoryGetByIndex(&state->inventory, next)->code;
}

void PageRows(ProgTP_AppState *state, int direction) {
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

void PrepareRows(ProgTP_AppState *state) {
    state->row_count = 0;
    state->filtered_count = 0;
    size_t selected_index = (size_t)-1;
    for (size_t i = 0; i < ProgTP_EquipmentInventoryGetCount(&state->inventory); ++i) {
        if (MatchesCurrentView(state, ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i))) {
            if (ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i)->code == state->selected_code) {
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
    for (size_t i = 0; i < ProgTP_EquipmentInventoryGetCount(&state->inventory); ++i) {
        if (!MatchesCurrentView(state, ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i))) {
            continue;
        }
        if (filtered_index >= state->row_offset && state->row_count < PROGTP_VISIBLE_ROWS) {
            AddRowText(state, ProgTP_EquipmentInventoryGetByIndex(&state->inventory, i));
        }
        ++filtered_index;
    }
    size_t first = state->row_offset + 1u;
    size_t last = state->row_offset + state->row_count;
    snprintf(state->row_page_text, sizeof(state->row_page_text), "Showing %zu-%zu of %zu", first, last, state->filtered_count);
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

void InventoryModule(ProgTP_AppState *state) {
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
