#include "app.h"
#include "app_internal.h"

#include "progtp_text.h"
#include "progtp_time.h"
#include "report_generator.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define FILES_ENTRY_COUNT 8
#define FILES_PREVIEW_MAX 128

typedef enum {
    FILE_TYPE_EQUIPMENT = 0,
    FILE_TYPE_INCIDENTS,
    FILE_TYPE_CONFIG,
    FILE_TYPE_SENSORS,
    FILE_TYPE_MONITORING,
    FILE_TYPE_PING_OUTPUT,
    FILE_TYPE_NETWORK_REPORT,
    FILE_TYPE_INCIDENT_REPORT,
} FileType;

typedef struct {
    char *path;
    FileType type;
    bool is_binary;
    const char *display_name;
    const char *type_label;
} FileDescriptor;

typedef struct {
    const FileDescriptor *descriptor;
    bool exists;
    size_t size;
    time_t modified;
} FileEntry;

static FileEntry g_files[FILES_ENTRY_COUNT];
static size_t g_files_count;
static char g_preview_buf[FILES_PREVIEW_MAX][192];
static size_t g_preview_count;
static char g_report_network_path[96];
static char g_report_incident_path[96];

static FileDescriptor FILE_TABLE[FILES_ENTRY_COUNT] = {
    { "equipamentos.dat",            FILE_TYPE_EQUIPMENT,      true,  "Inventory",       "Binary" },
    { "incidentes.dat",              FILE_TYPE_INCIDENTS,      true,  "Incidents",       "Binary" },
    { "configuracoes.dat",           FILE_TYPE_CONFIG,         true,  "Config history",  "Binary" },
    { "leituras_sensores.dat",       FILE_TYPE_SENSORS,        true,  "Sensor readings", "Binary" },
    { "log_monitorizacao.txt",       FILE_TYPE_MONITORING,     false, "Monitoring log",  "Text"   },
    { "resultado_ping.txt",          FILE_TYPE_PING_OUTPUT,    false, "Ping output",      "Text"   },
    { "relatorio_estado_rede.txt",   FILE_TYPE_NETWORK_REPORT,  false, "Network status report", "Text" },
    { "relatorio_incidentes.txt",    FILE_TYPE_INCIDENT_REPORT, false, "Incident report",  "Text" },
};

static void OnRefreshUpdateReportPaths(void) {
    char month_part[32];
    if (ProgTP_FormatReportMonth(month_part, sizeof(month_part))) {
        snprintf(g_report_network_path, sizeof(g_report_network_path), "relatorio_estado_rede_%s.txt", month_part);
        snprintf(g_report_incident_path, sizeof(g_report_incident_path), "relatorio_incidentes_%s.txt", month_part);
        ((FileDescriptor *)&FILE_TABLE[6])->path = g_report_network_path;
        ((FileDescriptor *)&FILE_TABLE[7])->path = g_report_incident_path;
    }
}

static void RefreshFileList(void) {
    OnRefreshUpdateReportPaths();
    g_files_count = 0;
    for (size_t i = 0; i < FILES_ENTRY_COUNT; ++i) {
        const FileDescriptor *desc = &FILE_TABLE[i];
        struct stat st;
        FileEntry *entry = &g_files[g_files_count++];
        memset(entry, 0, sizeof(*entry));
        entry->descriptor = desc;
        if (stat(desc->path, &st) == 0) {
            entry->exists = true;
            entry->size = (size_t)st.st_size;
            entry->modified = st.st_mtime;
        }
    }
}

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t next_value;
    uint64_t count;
} GenericBinaryHeader;

static bool ReadBinaryPreview(const char *path, size_t record_size, char *error, size_t error_size) {
    g_preview_count = 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, error_size, "Could not open %s", path);
        return false;
    }
    GenericBinaryHeader header;
    memset(&header, 0, sizeof(header));
    if (fread(&header, sizeof(header), 1u, file) != 1u) {
        fclose(file);
        snprintf(error, error_size, "Could not read header of %s", path);
        return false;
    }
    size_t header_size = sizeof(header);
    char magic_str[9];
    memcpy(magic_str, header.magic, 8u);
    magic_str[8u] = '\0';
    size_t count = (size_t)header.count;
    snprintf(
        g_preview_buf[0],
        sizeof(g_preview_buf[0]),
        "Header: %s v%u, %zu records",
        magic_str,
        header.version,
        count);
    g_preview_count = 1;
    if (record_size == 0) {
        record_size = 396u;
    }
    size_t display_count = count < (size_t)FILES_PREVIEW_MAX ? count : (size_t)FILES_PREVIEW_MAX;
    for (size_t i = 0; i < display_count && g_preview_count < FILES_PREVIEW_MAX; ++i) {
        size_t offset = header_size + i * record_size;
        if (fseek(file, (long)offset, SEEK_SET) != 0) {
            break;
        }
        ProgTP_Equipment equipment;
        memset(&equipment, 0, sizeof(equipment));
        if (fread(&equipment, record_size > sizeof(equipment) ? sizeof(equipment) : record_size, 1u, file) != 1u) {
            break;
        }
        equipment.name[sizeof(equipment.name) - 1u] = '\0';
        equipment.type[sizeof(equipment.type) - 1u] = '\0';
        equipment.brand[sizeof(equipment.brand) - 1u] = '\0';
        equipment.model[sizeof(equipment.model) - 1u] = '\0';
        equipment.ip_address[sizeof(equipment.ip_address) - 1u] = '\0';
        equipment.mac_address[sizeof(equipment.mac_address) - 1u] = '\0';
        equipment.location[sizeof(equipment.location) - 1u] = '\0';
        equipment.last_checked[sizeof(equipment.last_checked) - 1u] = '\0';
        ProgTP_TextSanitizePrintable(equipment.name);
        ProgTP_TextSanitizePrintable(equipment.type);
        ProgTP_TextSanitizePrintable(equipment.brand);
        ProgTP_TextSanitizePrintable(equipment.model);
        ProgTP_TextSanitizePrintable(equipment.ip_address);
        ProgTP_TextSanitizePrintable(equipment.mac_address);
        ProgTP_TextSanitizePrintable(equipment.location);
        ProgTP_TextSanitizePrintable(equipment.last_checked);
        snprintf(
            g_preview_buf[g_preview_count],
            sizeof(g_preview_buf[g_preview_count]),
            "#%u | %s | %s | %s %s | IP %s | %s",
            equipment.code,
            equipment.name,
            equipment.type,
            equipment.brand,
            equipment.model,
            equipment.ip_address,
            ProgTP_EquipmentStateName(equipment.state));
        ++g_preview_count;
    }
    fclose(file);
    return true;
}

static bool ReadTextPreview(const char *path, char *error, size_t error_size) {
    g_preview_count = 0;
    FILE *file = fopen(path, "r");
    if (!file) {
        snprintf(error, error_size, "Could not open %s", path);
        return false;
    }
    char line[256];
    while (g_preview_count < FILES_PREVIEW_MAX && fgets(line, sizeof(line), file)) {
        ProgTP_TextTrimRight(line);
        size_t len = strlen(line);
        if (len == 0) {
            snprintf(g_preview_buf[g_preview_count], sizeof(g_preview_buf[g_preview_count]), "%s", "(empty line)");
        } else {
            size_t copy_len = len < sizeof(g_preview_buf[0]) - 1u ? len : sizeof(g_preview_buf[0]) - 1u;
            memcpy(g_preview_buf[g_preview_count], line, copy_len);
            g_preview_buf[g_preview_count][copy_len] = '\0';
        }
        ++g_preview_count;
    }
    fclose(file);
    return true;
}

static void GeneratePreview(size_t index, char *error, size_t error_size) {
    g_preview_count = 0;
    if (index >= g_files_count) {
        snprintf(error, error_size, "No file selected");
        return;
    }
    const FileEntry *entry = &g_files[index];
    if (!entry->exists) {
        snprintf(error, error_size, "File does not exist on disk");
        return;
    }
    if (entry->descriptor->is_binary) {
        size_t record_size = 0;
        if (entry->size > sizeof(GenericBinaryHeader)) {
            record_size = (entry->size - sizeof(GenericBinaryHeader)) /
                (entry->size > sizeof(GenericBinaryHeader) + 396u ? 1u : 1u);
        }
        if (!ReadBinaryPreview(entry->descriptor->path, 0, error, error_size)) {
            return;
        }
    } else {
        if (!ReadTextPreview(entry->descriptor->path, error, error_size)) {
            return;
        }
    }
}

static bool FilesVisible(const ProgTP_AppState *state, const FileEntry *entry) {
    if (state->files_filter_state == 0) {
        return true;
    }
    if (state->files_filter_state == 1) {
        return entry->descriptor->is_binary;
    }
    return !entry->descriptor->is_binary;
}

static size_t CountVisible(const ProgTP_AppState *state) {
    size_t count = 0;
    for (size_t i = 0; i < g_files_count; ++i) {
        if (FilesVisible(state, &g_files[i])) {
            ++count;
        }
    }
    return count;
}

static bool SelectVisibleAt(ProgTP_AppState *state, size_t visible_index) {
    size_t current = 0;
    for (size_t i = 0; i < g_files_count; ++i) {
        if (!FilesVisible(state, &g_files[i])) {
            continue;
        }
        if (current == visible_index) {
            state->files_selected_index = i;
            return true;
        }
        ++current;
    }
    return false;
}

static void EnsureFilesSelection(ProgTP_AppState *state) {
    if (g_files_count == 0) {
        state->files_selected_index = 0;
        return;
    }
    if (state->files_selected_index >= g_files_count) {
        state->files_selected_index = g_files_count - 1u;
    }
}

void MoveFilesSelection(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisible(state);
    if (visible_count == 0) {
        EnsureFilesSelection(state);
        return;
    }
    size_t current = 0;
    bool found = false;
    for (size_t i = 0; i < g_files_count; ++i) {
        if (!FilesVisible(state, &g_files[i])) {
            continue;
        }
        if (i == state->files_selected_index) {
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
    if (next != current) {
        state->files_preview_loaded = false;
    }
    SelectVisibleAt(state, next);
}

void PageFiles(ProgTP_AppState *state, int direction) {
    size_t visible_count = CountVisible(state);
    if (visible_count <= PROGTP_VISIBLE_ROWS) {
        state->files_row_offset = 0;
        return;
    }
    if (direction > 0) {
        size_t next = state->files_row_offset + PROGTP_VISIBLE_ROWS;
        state->files_row_offset = next < visible_count ? next : 0;
    } else if (state->files_row_offset == 0) {
        state->files_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    } else {
        state->files_row_offset = state->files_row_offset > PROGTP_VISIBLE_ROWS
            ? state->files_row_offset - PROGTP_VISIBLE_ROWS
            : 0;
    }
    SelectVisibleAt(state, state->files_row_offset);
}

void PrepareFilesText(ProgTP_AppState *state) {
    if (state->files_needs_refresh) {
        state->files_needs_refresh = false;
        state->files_preview_loaded = false;
        RefreshFileList();
        EnsureFilesSelection(state);
        state->files_row_offset = 0;
    }

    size_t visible_count = CountVisible(state);

    snprintf(state->files_metric_total_text, sizeof(state->files_metric_total_text), "%zu files", visible_count);
    if (state->files_selected_index < g_files_count) {
        snprintf(
            state->files_metric_selected_text,
            sizeof(state->files_metric_selected_text),
            "%s",
            g_files[state->files_selected_index].descriptor->display_name);
    } else {
        snprintf(state->files_metric_selected_text, sizeof(state->files_metric_selected_text), "%s", "-");
    }

    EnsureSelection(state);

    if (g_files_count > 0 && state->files_selected_index < g_files_count) {
        const FileEntry *entry = &g_files[state->files_selected_index];
        char size_str[32];
        ProgTP_FormatFileSize(entry->size, size_str, sizeof(size_str));
        char mtime_str[32];
        if (entry->exists && entry->modified > 0) {
            ProgTP_FormatFileTimestamp(entry->modified, mtime_str, sizeof(mtime_str));
        } else {
            snprintf(mtime_str, sizeof(mtime_str), "%s", "-");
        }
        snprintf(
            state->files_selected_text,
            sizeof(state->files_selected_text),
            "Path:    %s\nType:    %s\nSize:    %s\nModified: %s\nExists:  %s",
            entry->descriptor->path,
            entry->descriptor->type_label,
            size_str,
            mtime_str,
            entry->exists ? "yes" : "no");
        if (!state->files_preview_loaded) {
            state->files_preview_loaded = true;
            char error[256] = {0};
            GeneratePreview(state->files_selected_index, error, sizeof(error));
            if (g_preview_count > 0) {
                state->files_preview_line_count = g_preview_count;
                for (size_t i = 0; i < g_preview_count && i < FILES_PREVIEW_MAX; ++i) {
                    snprintf(
                        state->files_preview_lines[i],
                        sizeof(state->files_preview_lines[i]),
                        "%s",
                        g_preview_buf[i]);
                }
            } else if (error[0] != '\0') {
                snprintf(state->files_preview_lines[0], sizeof(state->files_preview_lines[0]), "%s", error);
                state->files_preview_line_count = 1;
            } else {
                state->files_preview_line_count = 0;
            }
        }
    } else {
        snprintf(state->files_selected_text, sizeof(state->files_selected_text), "No file selected");
        state->files_preview_line_count = 0;
    }

    state->files_row_count = 0;
    if (visible_count == 0) {
        state->files_row_offset = 0;
        snprintf(state->files_row_page_text, sizeof(state->files_row_page_text), "%s",
            state->persistence_enabled ? "No files found" : "Files unavailable (in-memory mode)");
        return;
    }
    if (state->files_row_offset >= visible_count) {
        state->files_row_offset = ((visible_count - 1u) / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }

    size_t selected_visible_index = (size_t)-1;
    size_t visible_index = 0;
    for (size_t i = 0; i < g_files_count; ++i) {
        if (!FilesVisible(state, &g_files[i])) {
            continue;
        }
        if (i == state->files_selected_index) {
            selected_visible_index = visible_index;
            break;
        }
        ++visible_index;
    }
    if (selected_visible_index != (size_t)-1 &&
        (selected_visible_index < state->files_row_offset ||
         selected_visible_index >= state->files_row_offset + PROGTP_VISIBLE_ROWS)) {
        state->files_row_offset = (selected_visible_index / PROGTP_VISIBLE_ROWS) * PROGTP_VISIBLE_ROWS;
    }

    visible_index = 0;
    for (size_t i = 0; i < g_files_count && state->files_row_count < PROGTP_VISIBLE_ROWS; ++i) {
        const FileEntry *entry = &g_files[i];
        if (!FilesVisible(state, entry)) {
            continue;
        }
        if (visible_index >= state->files_row_offset) {
            size_t row = state->files_row_count++;
            char size_str[32];
            char mtime_str[32];
            if (entry->exists) {
                ProgTP_FormatFileSize(entry->size, size_str, sizeof(size_str));
                ProgTP_FormatFileTimestamp(entry->modified, mtime_str, sizeof(mtime_str));
            } else {
                snprintf(size_str, sizeof(size_str), "%s", "-");
                snprintf(mtime_str, sizeof(mtime_str), "%s", "-");
            }
            snprintf(
                state->files_row_texts[row],
                sizeof(state->files_row_texts[row]),
                "%s | %s | %s | %s",
                entry->descriptor->display_name,
                entry->descriptor->type_label,
                size_str,
                mtime_str);
        }
        ++visible_index;
    }
    snprintf(
        state->files_row_page_text,
        sizeof(state->files_row_page_text),
        "Showing %zu-%zu of %zu",
        state->files_row_offset + 1u,
        state->files_row_offset + state->files_row_count,
        visible_count);
}

static void FilesTable(ProgTP_AppState *state) {
    CLAY(CLAY_ID("FilesTable"), {
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
        CLAY(CLAY_ID("FilesTableHeader"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(ControlHeight() + 8.0f) },
                .padding = { 10, 10, 4, 4 },
                .childGap = 8,
                .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = COLOR_SURFACE_DARK,
        }) {
            CLAY(CLAY_ID("FilesTableTitle"), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .childGap = 2,
                },
            }) {
                TextLine("Data files", 13, COLOR_TEXT);
                TextLine(state->files_row_page_text, 12, COLOR_MUTED);
            }
            Button(900, "Prev Page", PROGTP_APP_ACTION_FILES_PAGE_PREVIOUS, false, false);
            Button(901, "Next Page", PROGTP_APP_ACTION_FILES_PAGE_NEXT, false, false);
        }
        for (size_t i = 0; i < state->files_row_count; ++i) {
            size_t entry_index = (size_t)-1;
            size_t visible_index = 0;
            for (size_t j = 0; j < g_files_count; ++j) {
                if (!FilesVisible(state, &g_files[j])) {
                    continue;
                }
                if (visible_index == state->files_row_offset + i) {
                    entry_index = j;
                    break;
                }
                ++visible_index;
            }
            if (entry_index == (size_t)-1) {
                continue;
            }
            bool selected = entry_index == state->files_selected_index;
            CLAY(CLAY_IDI("FilesRow", (uint32_t)i), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32) },
                    .padding = { 10, 10, 0, 0 },
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                },
                .backgroundColor = selected ? COLOR_ACCENT : (i % 2u == 0 ? COLOR_SURFACE : COLOR_SURFACE_ALT),
            }) {
                AttachInteraction(PROGTP_UI_FILES_SELECT_BASE + (uintptr_t)entry_index);
                TextLine(state->files_row_texts[i], 12, selected ? COLOR_WHITE : COLOR_TEXT);
            }
        }
    }
}

static void FilesDetailPanel(ProgTP_AppState *state) {
    CLAY(CLAY_ID("FilesDetailPanel"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = {
                progtp_ui_compact ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(420.0f),
                CLAY_SIZING_GROW(.min = 180.0f),
            },
            .padding = CLAY_PADDING_ALL(14),
            .childGap = 6,
        },
        .backgroundColor = COLOR_SURFACE,
        .border = { .color = COLOR_LINE, .width = { 1, 1, 1, 1, 0 } },
        .cornerRadius = CLAY_CORNER_RADIUS(5),
    }) {
        TextLine("Selected file", 16, COLOR_TEXT);
        if (g_files_count > 0 && state->files_selected_index < g_files_count) {
            TextLine(state->files_selected_text, 12, COLOR_TEXT);
            if (state->files_preview_line_count > 0) {
                CLAY(CLAY_ID("FilesPreviewSeparator"), {
                    .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(4) } },
                }) {}
                TextLine("Content", 14, COLOR_TEXT);
                for (size_t i = 0; i < state->files_preview_line_count; ++i) {
                    TextLine(state->files_preview_lines[i], 11, COLOR_MUTED);
                }
            }
        } else {
            TextLine("No file selected", 13, COLOR_MUTED);
        }
        if (!state->persistence_enabled) {
            TextLine("Files not available in web mode", 12, COLOR_ACCENT_DARK);
        }
        if (state->status[0] != '\0') {
            TextLine(state->status, 13, COLOR_MUTED);
        }
    }
}

size_t ProgTP_AppGetFilesCount(void) {
    return g_files_count;
}

void FilesModule(ProgTP_AppState *state) {
    CLAY(CLAY_ID("FilesModule"), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .childGap = progtp_ui_compact ? 8 : 12,
        },
    }) {
        CLAY(CLAY_ID("FilesMetrics"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = 10,
            },
        }) {
            Metric("Total", state->files_metric_total_text);
            Metric("Selected", state->files_metric_selected_text);
        }
        CLAY(CLAY_ID("FilesActions"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(910, "Refresh", PROGTP_APP_ACTION_FILES_REFRESH, true, false);
            Button(911, "Prev", PROGTP_APP_ACTION_FILES_PREVIOUS, g_files_count > 1, false);
            Button(912, "Next", PROGTP_APP_ACTION_FILES_NEXT, g_files_count > 1, false);
        }
        CLAY(CLAY_ID("FilesActionsRow2"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(913, "Generate Network Report", PROGTP_APP_ACTION_FILES_GENERATE_NETWORK_REPORT, state->persistence_enabled, false);
            Button(914, "Generate Incident Report", PROGTP_APP_ACTION_FILES_GENERATE_INCIDENT_REPORT, state->persistence_enabled, false);
        }
        CLAY(CLAY_ID("FilesFilters"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                .childGap = ControlGap(),
            },
        }) {
            Button(920, "All", PROGTP_APP_ACTION_FILES_FILTER_ALL, state->files_filter_state == 0, false);
            Button(921, "Binary", PROGTP_APP_ACTION_FILES_FILTER_BINARY, state->files_filter_state == 1, false);
            Button(922, "Text", PROGTP_APP_ACTION_FILES_FILTER_TEXT, state->files_filter_state == 2, false);
        }
        CLAY(CLAY_ID("FilesContent"), {
            .layout = {
                .layoutDirection = progtp_ui_compact ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 12,
            },
        }) {
            FilesTable(state);
            FilesDetailPanel(state);
        }
    }
}
