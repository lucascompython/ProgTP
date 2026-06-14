#include "config_history.h"

#include "progtp_error.h"
#include "progtp_text.h"
#include "progtp_time.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGTP_CONFIG_FILE_MAGIC "PTPCFG1"
#define PROGTP_CONFIG_FILE_VERSION 1u

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t next_id;
    uint64_t count;
    uint64_t undo_index;
} ProgTP_ConfigFileHeader;

void ProgTP_ConfigHistoryInit(ProgTP_ConfigHistory *history) {
    memset(history, 0, sizeof(*history));
    history->undo_index = 0;
}

void ProgTP_ConfigHistoryDestroy(ProgTP_ConfigHistory *history) {
    free(history->items);
    memset(history, 0, sizeof(*history));
}

void ProgTP_ConfigHistoryClear(ProgTP_ConfigHistory *history) {
    history->length = 0;
    history->undo_index = 0;
}

bool ProgTP_ConfigHistoryCopy(ProgTP_ConfigHistory *destination, const ProgTP_ConfigHistory *source, char *error, size_t error_size) {
    if (!destination) {
        ProgTP_SetError(error, error_size, "missing destination config history");
        return false;
    }
    free(destination->items);
    destination->items = NULL;
    destination->length = 0;
    destination->capacity = 0;
    destination->next_id = 1;
    destination->undo_index = 0;
    if (source && source->length > 0) {
        destination->items = calloc(source->length, sizeof(*destination->items));
        if (!destination->items) {
            ProgTP_SetError(error, error_size, "not enough memory for config history copy");
            return false;
        }
        memcpy(destination->items, source->items, source->length * sizeof(*destination->items));
        destination->capacity = source->length;
        destination->length = source->length;
        destination->next_id = source->next_id;
        destination->undo_index = source->undo_index;
    }
    return true;
}

static bool EnsureCapacity(ProgTP_ConfigHistory *history, size_t minimum, char *error, size_t error_size) {
    if (history->capacity >= minimum) {
        return true;
    }
    size_t capacity = history->capacity == 0 ? 8u : history->capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2u) {
            ProgTP_SetError(error, error_size, "config history is too large");
            return false;
        }
        capacity *= 2u;
    }
    ProgTP_ConfigEntry *items = realloc(history->items, capacity * sizeof(*items));
    if (!items) {
        ProgTP_SetError(error, error_size, "not enough memory for config history");
        return false;
    }
    memset(items + history->length, 0, (capacity - history->length) * sizeof(*items));
    history->items = items;
    history->capacity = capacity;
    return true;
}

static void NullTerminateEntry(ProgTP_ConfigEntry *entry) {
    entry->equipment_name[sizeof(entry->equipment_name) - 1u] = '\0';
    entry->timestamp[sizeof(entry->timestamp) - 1u] = '\0';
    entry->description[sizeof(entry->description) - 1u] = '\0';
    entry->before.name[sizeof(entry->before.name) - 1u] = '\0';
    entry->before.type[sizeof(entry->before.type) - 1u] = '\0';
    entry->before.brand[sizeof(entry->before.brand) - 1u] = '\0';
    entry->before.model[sizeof(entry->before.model) - 1u] = '\0';
    entry->before.ip_address[sizeof(entry->before.ip_address) - 1u] = '\0';
    entry->before.mac_address[sizeof(entry->before.mac_address) - 1u] = '\0';
    entry->before.location[sizeof(entry->before.location) - 1u] = '\0';
    entry->before.last_checked[sizeof(entry->before.last_checked) - 1u] = '\0';
    entry->after.name[sizeof(entry->after.name) - 1u] = '\0';
    entry->after.type[sizeof(entry->after.type) - 1u] = '\0';
    entry->after.brand[sizeof(entry->after.brand) - 1u] = '\0';
    entry->after.model[sizeof(entry->after.model) - 1u] = '\0';
    entry->after.ip_address[sizeof(entry->after.ip_address) - 1u] = '\0';
    entry->after.mac_address[sizeof(entry->after.mac_address) - 1u] = '\0';
    entry->after.location[sizeof(entry->after.location) - 1u] = '\0';
    entry->after.last_checked[sizeof(entry->after.last_checked) - 1u] = '\0';
}

bool ProgTP_ConfigHistoryLoad(ProgTP_ConfigHistory *history, const char *path, char *error, size_t error_size) {
    if (!history || !path) {
        ProgTP_SetError(error, error_size, "missing config history or path");
        return false;
    }
    ProgTP_ConfigHistoryClear(history);
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) {
            return true;
        }
        ProgTP_SetError(error, error_size, "could not open config history binary file");
        return false;
    }
    ProgTP_ConfigFileHeader header;
    if (fread(&header, sizeof(header), 1u, file) != 1u ||
        memcmp(header.magic, PROGTP_CONFIG_FILE_MAGIC, sizeof(header.magic) - 1u) != 0 ||
        header.version != PROGTP_CONFIG_FILE_VERSION ||
        header.count > SIZE_MAX / sizeof(ProgTP_ConfigEntry)) {
        fclose(file);
        ProgTP_SetError(error, error_size, "invalid config history binary file");
        return false;
    }
    if (!EnsureCapacity(history, (size_t)header.count, error, error_size)) {
        fclose(file);
        return false;
    }
    if (header.count > 0 &&
        fread(history->items, sizeof(history->items[0]), (size_t)header.count, file) != (size_t)header.count) {
        fclose(file);
        ProgTP_SetError(error, error_size, "could not read config history binary file");
        return false;
    }
    history->length = (size_t)header.count;
    history->next_id = header.next_id == 0 ? 1u : header.next_id;
    if (header.undo_index > history->length) {
        history->undo_index = history->length;
    } else {
        history->undo_index = (size_t)header.undo_index;
    }
    for (size_t i = 0; i < history->length; ++i) {
        NullTerminateEntry(&history->items[i]);
    }
    fclose(file);
    return true;
}

bool ProgTP_ConfigHistorySave(const ProgTP_ConfigHistory *history, const char *path, char *error, size_t error_size) {
    if (!history || !path) {
        ProgTP_SetError(error, error_size, "missing config history or path");
        return false;
    }
    FILE *file = fopen(path, "wb");
    if (!file) {
        ProgTP_SetError(error, error_size, "could not create config history binary file");
        return false;
    }
    ProgTP_ConfigFileHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, PROGTP_CONFIG_FILE_MAGIC, sizeof(header.magic) - 1u);
    header.version = PROGTP_CONFIG_FILE_VERSION;
    header.next_id = history->next_id == 0 ? 1u : history->next_id;
    header.count = (uint64_t)history->length;
    header.undo_index = (uint64_t)history->undo_index;
    bool ok = fwrite(&header, sizeof(header), 1u, file) == 1u;
    if (ok && history->length > 0) {
        ok = fwrite(history->items, sizeof(history->items[0]), history->length, file) == history->length;
    }
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        ProgTP_SetError(error, error_size, "could not write config history binary file");
    }
    return ok;
}

bool ProgTP_ConfigHistoryRecord(
    ProgTP_ConfigHistory *history,
    ProgTP_ConfigOpType op_type,
    const ProgTP_Equipment *before,
    const ProgTP_Equipment *after,
    const char *description,
    char *error,
    size_t error_size) {
    if (!history || !after) {
        ProgTP_SetError(error, error_size, "missing config history or after snapshot");
        return false;
    }

    if (history->undo_index < history->length) {
        for (size_t i = history->undo_index; i < history->length; ++i) {
            if (history->items[i].id >= history->next_id) {
                history->next_id = history->items[i].id + 1u;
            }
        }
        history->length = history->undo_index;
    }

    if (history->length >= PROGTP_CONFIG_HISTORY_LIMIT) {
        size_t shift = history->length - PROGTP_CONFIG_HISTORY_LIMIT + 1u;
        memmove(
            &history->items[0],
            &history->items[shift],
            (history->length - shift) * sizeof(history->items[0]));
        history->length -= shift;
        if (history->undo_index >= shift) {
            history->undo_index -= shift;
        } else {
            history->undo_index = 0;
        }
    }

    if (!EnsureCapacity(history, history->length + 1u, error, error_size)) {
        return false;
    }

    ProgTP_ConfigEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.id = history->next_id;
    entry.equipment_code = after->code;
    ProgTP_TextCopy(entry.equipment_name, sizeof(entry.equipment_name), after->name);
    ProgTP_FormatCurrentTimestamp(entry.timestamp, sizeof(entry.timestamp));
    if (ProgTP_TextIsEmpty(entry.timestamp)) {
        ProgTP_TextCopy(entry.timestamp, sizeof(entry.timestamp), "unknown");
    }
    entry.op_type = op_type;
    ProgTP_TextCopy(entry.description, sizeof(entry.description), description);
    if (before) {
        entry.before = *before;
    }
    entry.after = *after;
    entry.entry_state = PROGTP_CONFIG_ENTRY_APPLIED;
    NullTerminateEntry(&entry);

    history->items[history->length++] = entry;
    history->undo_index = history->length;
    if (history->next_id < UINT32_MAX) {
        ++history->next_id;
    }
    return true;
}

bool ProgTP_ConfigHistoryApplySnapshot(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_Equipment *snapshot,
    char *error,
    size_t error_size) {
    return ProgTP_EquipmentInventoryApplySnapshot(inventory, snapshot, error, error_size);
}

bool ProgTP_ConfigHistoryUndo(
    ProgTP_ConfigHistory *history,
    ProgTP_EquipmentInventory *inventory,
    char *error,
    size_t error_size) {
    if (!history || !inventory) {
        ProgTP_SetError(error, error_size, "missing config history or inventory");
        return false;
    }
    if (history->undo_index == 0) {
        ProgTP_SetError(error, error_size, "nothing to undo");
        return false;
    }
    size_t index = history->undo_index - 1u;
    ProgTP_ConfigEntry *entry = &history->items[index];
    const ProgTP_Equipment *snapshot = &entry->before;
    if (snapshot->code == 0) {
        snapshot = &entry->after;
    }
    if (!ProgTP_EquipmentInventoryApplySnapshot(inventory, snapshot, error, error_size)) {
        return false;
    }
    entry->entry_state = PROGTP_CONFIG_ENTRY_UNDONE;
    if (history->undo_index > 0) {
        --history->undo_index;
    }
    return true;
}

bool ProgTP_ConfigHistoryRedo(
    ProgTP_ConfigHistory *history,
    ProgTP_EquipmentInventory *inventory,
    char *error,
    size_t error_size) {
    if (!history || !inventory) {
        ProgTP_SetError(error, error_size, "missing config history or inventory");
        return false;
    }
    if (history->undo_index >= history->length) {
        ProgTP_SetError(error, error_size, "nothing to redo");
        return false;
    }
    ProgTP_ConfigEntry *entry = &history->items[history->undo_index];
    if (!ProgTP_EquipmentInventoryApplySnapshot(inventory, &entry->after, error, error_size)) {
        return false;
    }
    entry->entry_state = PROGTP_CONFIG_ENTRY_APPLIED;
    ++history->undo_index;
    return true;
}

bool ProgTP_ConfigHistoryCanUndo(const ProgTP_ConfigHistory *history) {
    return history && history->undo_index > 0;
}

bool ProgTP_ConfigHistoryCanRedo(const ProgTP_ConfigHistory *history) {
    return history && history->undo_index < history->length;
}

size_t ProgTP_ConfigHistoryAppliedCount(const ProgTP_ConfigHistory *history) {
    return history ? history->undo_index : 0u;
}

size_t ProgTP_ConfigHistoryUndoneCount(const ProgTP_ConfigHistory *history) {
    if (!history) {
        return 0u;
    }
    return history->length > history->undo_index ? history->length - history->undo_index : 0u;
}

bool ProgTP_ConfigHistoryDeleteById(
    ProgTP_ConfigHistory *history,
    uint32_t id,
    char *error,
    size_t error_size) {
    if (!history) {
        ProgTP_SetError(error, error_size, "missing config history");
        return false;
    }
    for (size_t i = 0; i < history->length; ++i) {
        if (history->items[i].id == id) {
            if (i < history->undo_index) {
                --history->undo_index;
            }
            memmove(
                &history->items[i],
                &history->items[i + 1u],
                (history->length - i - 1u) * sizeof(history->items[0]));
            --history->length;
            return true;
        }
    }
    ProgTP_SetError(error, error_size, "config entry not found");
    return false;
}

bool ProgTP_ConfigHistoryImportFromFile(
    ProgTP_ConfigHistory *history,
    const char *path,
    char *error,
    size_t error_size) {
    if (!history || !path) {
        ProgTP_SetError(error, error_size, "missing config history or path");
        return false;
    }
    ProgTP_ConfigHistory imported;
    ProgTP_ConfigHistoryInit(&imported);
    if (!ProgTP_ConfigHistoryLoad(&imported, path, error, error_size)) {
        ProgTP_ConfigHistoryDestroy(&imported);
        return false;
    }
    uint32_t base_id = history->next_id;
    for (size_t i = 0; i < imported.length; ++i) {
        ProgTP_ConfigEntry *entry = &imported.items[i];
        entry->id = base_id++;
        if (history->next_id <= entry->id) {
            history->next_id = entry->id + 1u;
        }
        char append_error[256] = {0};
        if (!ProgTP_ConfigHistoryRecord(
                history,
                entry->op_type,
                &entry->before,
                &entry->after,
                entry->description,
                append_error,
                sizeof(append_error))) {
            ProgTP_SetError(error, error_size, append_error[0] ? append_error : "could not import config entry");
            ProgTP_ConfigHistoryDestroy(&imported);
            return false;
        }
        size_t last = history->length - 1u;
        history->items[last].equipment_code = entry->equipment_code;
        ProgTP_TextCopy(
            history->items[last].equipment_name,
            sizeof(history->items[last].equipment_name),
            entry->equipment_name);
        ProgTP_TextCopy(
            history->items[last].timestamp,
            sizeof(history->items[last].timestamp),
            entry->timestamp);
        history->items[last].entry_state = entry->entry_state;
    }
    ProgTP_ConfigHistoryDestroy(&imported);
    return true;
}

const char *ProgTP_ConfigOpName(ProgTP_ConfigOpType op_type) {
    switch (op_type) {
        case PROGTP_CONFIG_OP_ADD: return "Add";
        case PROGTP_CONFIG_OP_UPDATE: return "Update";
        case PROGTP_CONFIG_OP_REMOVE: return "Remove";
        case PROGTP_CONFIG_OP_SET_STATE: return "State change";
        case PROGTP_CONFIG_OP_SET_PENDING: return "Pending toggle";
    }
    return "Unknown";
}

const char *ProgTP_ConfigEntryStateName(ProgTP_ConfigEntryState state) {
    switch (state) {
        case PROGTP_CONFIG_ENTRY_APPLIED: return "applied";
        case PROGTP_CONFIG_ENTRY_UNDONE: return "undone";
    }
    return "unknown";
}

void ProgTP_ConfigHistoryFormatRow(const ProgTP_ConfigEntry *entry, char *buffer, size_t buffer_size) {
    if (!entry) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }
    snprintf(
        buffer,
        buffer_size,
        "#%u | %s | #%u %s | %s | %s",
        entry->id,
        entry->timestamp,
        entry->equipment_code,
        entry->equipment_name,
        ProgTP_ConfigOpName(entry->op_type),
        ProgTP_ConfigEntryStateName(entry->entry_state));
}

void ProgTP_ConfigHistoryFormatDetail(const ProgTP_ConfigEntry *entry, char *buffer, size_t buffer_size) {
    if (!entry) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }
    int written = snprintf(
        buffer,
        buffer_size,
        "Entry #%u\nOperation: %s\nStatus: %s\nTime: %s\nEquipment: #%u %s\n\n%s\n\nBefore: %s | %s | %s %s | IP %s | MAC %s | %s | %s\nAfter:  %s | %s | %s %s | IP %s | MAC %s | %s | %s",
        entry->id,
        ProgTP_ConfigOpName(entry->op_type),
        ProgTP_ConfigEntryStateName(entry->entry_state),
        entry->timestamp,
        entry->equipment_code,
        entry->equipment_name,
        entry->description,
        entry->before.name[0] ? entry->before.name : "(none)",
        entry->before.type[0] ? entry->before.type : "(none)",
        entry->before.brand,
        entry->before.model,
        entry->before.ip_address,
        entry->before.mac_address,
        entry->before.location,
        ProgTP_EquipmentStateName(entry->before.state),
        entry->after.name,
        entry->after.type,
        entry->after.brand,
        entry->after.model,
        entry->after.ip_address,
        entry->after.mac_address,
        entry->after.location,
        ProgTP_EquipmentStateName(entry->after.state));
    (void)written;
}
