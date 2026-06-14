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
    uint64_t undo_count;
} ProgTP_ConfigFileHeader;

static ProgTP_ConfigNode *GetNodeAt(ProgTP_ConfigHistory *history, size_t index) {
    ProgTP_ConfigNode *current = history->top;
    for (size_t i = 0; i < index && current; ++i) {
        current = current->next;
    }
    return current;
}

static const ProgTP_ConfigNode *GetNodeAtConst(const ProgTP_ConfigHistory *history, size_t index) {
    const ProgTP_ConfigNode *current = history->top;
    for (size_t i = 0; i < index && current; ++i) {
        current = current->next;
    }
    return current;
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

void ProgTP_ConfigHistoryInit(ProgTP_ConfigHistory *history) {
    memset(history, 0, sizeof(*history));
}

void ProgTP_ConfigHistoryDestroy(ProgTP_ConfigHistory *history) {
    free(history->top);
    memset(history, 0, sizeof(*history));
}

void ProgTP_ConfigHistoryClear(ProgTP_ConfigHistory *history) {
    ProgTP_ConfigNode *current = history->top;
    while (current) {
        ProgTP_ConfigNode *next = current->next;
        free(current);
        current = next;
    }
    history->top = NULL;
    history->length = 0;
    history->undo_count = 0;
}

bool ProgTP_ConfigHistoryCopy(ProgTP_ConfigHistory *destination, const ProgTP_ConfigHistory *source, char *error, size_t error_size) {
    if (!destination) {
        ProgTP_SetError(error, error_size, "missing destination config history");
        return false;
    }
    ProgTP_ConfigHistoryClear(destination);
    destination->next_id = 1;
    if (source && source->length > 0) {
        ProgTP_ConfigNode *new_top = NULL;
        ProgTP_ConfigNode *prev = NULL;
        const ProgTP_ConfigNode *current = source->top;
        while (current) {
            ProgTP_ConfigNode *node = calloc(1u, sizeof(*node));
            if (!node) {
                ProgTP_ConfigNode *n = new_top;
                while (n) {
                    ProgTP_ConfigNode *next = n->next;
                    free(n);
                    n = next;
                }
                ProgTP_SetError(error, error_size, "not enough memory for config history copy");
                return false;
            }
            node->entry = current->entry;
            node->next = NULL;
            if (!new_top) {
                new_top = node;
            }
            if (prev) {
                prev->next = node;
            }
            prev = node;
            current = current->next;
        }
        destination->top = new_top;
        destination->length = source->length;
        destination->next_id = source->next_id;
        destination->undo_count = source->undo_count;
    }
    return true;
}

size_t ProgTP_ConfigHistoryGetCount(const ProgTP_ConfigHistory *history) {
    return history ? history->length : 0u;
}

const ProgTP_ConfigEntry *ProgTP_ConfigHistoryGetByIndex(const ProgTP_ConfigHistory *history, size_t index) {
    const ProgTP_ConfigNode *node = GetNodeAtConst(history, index);
    return node ? &node->entry : NULL;
}

ProgTP_ConfigEntry *ProgTP_ConfigHistoryGetByIndexMut(ProgTP_ConfigHistory *history, size_t index) {
    ProgTP_ConfigNode *node = GetNodeAt(history, index);
    return node ? &node->entry : NULL;
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

    ProgTP_ConfigEntry *temp = NULL;
    size_t count = (size_t)header.count;
    if (count > 0) {
        temp = calloc(count, sizeof(*temp));
        if (!temp || fread(temp, sizeof(*temp), count, file) != count) {
            free(temp);
            fclose(file);
            ProgTP_SetError(error, error_size, "could not read config history binary file");
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            NullTerminateEntry(&temp[i]);
        }
    }
    fclose(file);

    for (size_t i = count; i > 0; --i) {
        ProgTP_ConfigNode *node = calloc(1u, sizeof(*node));
        if (!node) {
            free(temp);
            ProgTP_ConfigHistoryClear(history);
            ProgTP_SetError(error, error_size, "not enough memory for config history");
            return false;
        }
        node->entry = temp[i - 1u];
        node->next = history->top;
        history->top = node;
    }
    free(temp);

    history->length = count;
    history->next_id = header.next_id == 0 ? 1u : header.next_id;
    history->undo_count = (size_t)header.undo_count;
    if (history->undo_count > history->length) {
        history->undo_count = 0;
    }
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
    header.undo_count = (uint64_t)history->undo_count;

    bool ok = fwrite(&header, sizeof(header), 1u, file) == 1u;
    if (ok && history->length > 0) {
        ProgTP_ConfigEntry *temp = calloc(history->length, sizeof(*temp));
        if (temp) {
            size_t idx = 0;
            const ProgTP_ConfigNode *current = history->top;
            while (current && idx < history->length) {
                temp[idx++] = current->entry;
                current = current->next;
            }
            ok = fwrite(temp, sizeof(*temp), history->length, file) == history->length;
            free(temp);
        } else {
            ok = false;
        }
    }
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        ProgTP_SetError(error, error_size, "could not write config history binary file");
    }
    return ok;
}

bool ProgTP_ConfigHistoryPush(
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

    while (history->undo_count > 0) {
        ProgTP_ConfigNode *node = history->top;
        if (!node) {
            break;
        }
        if (node->entry.id >= history->next_id) {
            history->next_id = node->entry.id + 1u;
        }
        history->top = node->next;
        free(node);
        --history->length;
        --history->undo_count;
    }

    if (history->length >= PROGTP_CONFIG_HISTORY_LIMIT) {
        ProgTP_ConfigNode *prev = NULL;
        ProgTP_ConfigNode *current = history->top;
        size_t skip = history->length - PROGTP_CONFIG_HISTORY_LIMIT + 1u;
        size_t traversed = 0;
        while (current && traversed < history->length - skip) {
            prev = current;
            current = current->next;
            ++traversed;
        }
        if (!prev && traversed == 0) {
            ProgTP_ConfigNode *removed = history->top;
            if (removed) {
                history->top = removed->next;
                free(removed);
                --history->length;
            }
        } else if (prev) {
            for (size_t i = 0; i < skip && current; ++i) {
                ProgTP_ConfigNode *next = current->next;
                free(current);
                current = next;
                --history->length;
            }
            prev->next = current;
        }
    }

    ProgTP_ConfigNode *node = calloc(1u, sizeof(*node));
    if (!node) {
        ProgTP_SetError(error, error_size, "not enough memory for config entry");
        return false;
    }

    node->entry.id = history->next_id;
    node->entry.equipment_code = after->code;
    ProgTP_TextCopy(node->entry.equipment_name, sizeof(node->entry.equipment_name), after->name);
    ProgTP_FormatCurrentTimestamp(node->entry.timestamp, sizeof(node->entry.timestamp));
    if (ProgTP_TextIsEmpty(node->entry.timestamp)) {
        ProgTP_TextCopy(node->entry.timestamp, sizeof(node->entry.timestamp), "unknown");
    }
    node->entry.op_type = op_type;
    ProgTP_TextCopy(node->entry.description, sizeof(node->entry.description), description);
    if (before) {
        node->entry.before = *before;
    }
    node->entry.after = *after;
    node->entry.entry_state = PROGTP_CONFIG_ENTRY_APPLIED;
    NullTerminateEntry(&node->entry);

    node->next = history->top;
    history->top = node;
    ++history->length;
    history->undo_count = 0;

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
    if (!ProgTP_ConfigHistoryCanUndo(history)) {
        ProgTP_SetError(error, error_size, "nothing to undo");
        return false;
    }
    ProgTP_ConfigNode *node = GetNodeAt(history, history->undo_count);
    if (!node) {
        ProgTP_SetError(error, error_size, "undo entry not found");
        return false;
    }
    const ProgTP_Equipment *snapshot = &node->entry.before;
    if (snapshot->code == 0) {
        snapshot = &node->entry.after;
    }
    if (!ProgTP_EquipmentInventoryApplySnapshot(inventory, snapshot, error, error_size)) {
        return false;
    }
    node->entry.entry_state = PROGTP_CONFIG_ENTRY_UNDONE;
    ++history->undo_count;
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
    if (!ProgTP_ConfigHistoryCanRedo(history)) {
        ProgTP_SetError(error, error_size, "nothing to redo");
        return false;
    }
    ProgTP_ConfigNode *node = GetNodeAt(history, history->undo_count - 1u);
    if (!node) {
        ProgTP_SetError(error, error_size, "redo entry not found");
        return false;
    }
    if (!ProgTP_EquipmentInventoryApplySnapshot(inventory, &node->entry.after, error, error_size)) {
        return false;
    }
    node->entry.entry_state = PROGTP_CONFIG_ENTRY_APPLIED;
    --history->undo_count;
    return true;
}

bool ProgTP_ConfigHistoryCanUndo(const ProgTP_ConfigHistory *history) {
    return history && history->undo_count < history->length &&
        GetNodeAtConst(history, history->undo_count) != NULL &&
        GetNodeAtConst(history, history->undo_count)->entry.entry_state == PROGTP_CONFIG_ENTRY_APPLIED;
}

bool ProgTP_ConfigHistoryCanRedo(const ProgTP_ConfigHistory *history) {
    return history && history->undo_count > 0 &&
        GetNodeAtConst(history, history->undo_count - 1u) != NULL &&
        GetNodeAtConst(history, history->undo_count - 1u)->entry.entry_state == PROGTP_CONFIG_ENTRY_UNDONE;
}

size_t ProgTP_ConfigHistoryAppliedCount(const ProgTP_ConfigHistory *history) {
    if (!history) {
        return 0u;
    }
    return history->length - history->undo_count;
}

size_t ProgTP_ConfigHistoryUndoneCount(const ProgTP_ConfigHistory *history) {
    if (!history) {
        return 0u;
    }
    return history->undo_count;
}

bool ProgTP_ConfigHistoryPop(
    ProgTP_ConfigHistory *history,
    uint32_t id,
    char *error,
    size_t error_size) {
    if (!history) {
        ProgTP_SetError(error, error_size, "missing config history");
        return false;
    }
    ProgTP_ConfigNode *prev = NULL;
    ProgTP_ConfigNode *current = history->top;
    size_t index = 0;
    while (current) {
        if (current->entry.id == id) {
            if (index < history->undo_count) {
                --history->undo_count;
            }
            if (prev) {
                prev->next = current->next;
            } else {
                history->top = current->next;
            }
            free(current);
            --history->length;
            return true;
        }
        prev = current;
        current = current->next;
        ++index;
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
    const ProgTP_ConfigNode *current = imported.top;
    while (current) {
        ProgTP_ConfigEntry entry = current->entry;
        entry.id = base_id++;
        if (history->next_id <= entry.id) {
            history->next_id = entry.id + 1u;
        }
        char push_error[256] = {0};
        if (!ProgTP_ConfigHistoryPush(
                history,
                entry.op_type,
                &entry.before,
                &entry.after,
                entry.description,
                push_error,
                sizeof(push_error))) {
            ProgTP_SetError(error, error_size, push_error[0] ? push_error : "could not import config entry");
            ProgTP_ConfigHistoryDestroy(&imported);
            return false;
        }
        if (history->top) {
            history->top->entry.equipment_code = entry.equipment_code;
            ProgTP_TextCopy(history->top->entry.equipment_name, sizeof(history->top->entry.equipment_name), entry.equipment_name);
            ProgTP_TextCopy(history->top->entry.timestamp, sizeof(history->top->entry.timestamp), entry.timestamp);
            history->top->entry.entry_state = entry.entry_state;
        }
        current = current->next;
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
