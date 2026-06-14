#ifndef PROGTP_CONFIG_HISTORY_H
#define PROGTP_CONFIG_HISTORY_H

#include "equipment_inventory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGTP_CONFIG_TIMESTAMP_SIZE 24u
#define PROGTP_CONFIG_DESCRIPTION_SIZE 160u
#define PROGTP_CONFIG_HISTORY_LIMIT 25u

typedef enum {
    PROGTP_CONFIG_OP_ADD = 0,
    PROGTP_CONFIG_OP_UPDATE,
    PROGTP_CONFIG_OP_REMOVE,
    PROGTP_CONFIG_OP_SET_STATE,
    PROGTP_CONFIG_OP_SET_PENDING,
} ProgTP_ConfigOpType;

typedef enum {
    PROGTP_CONFIG_ENTRY_APPLIED = 0,
    PROGTP_CONFIG_ENTRY_UNDONE,
} ProgTP_ConfigEntryState;

typedef struct {
    uint32_t id;
    uint32_t equipment_code;
    char equipment_name[PROGTP_EQUIPMENT_NAME_SIZE];
    char timestamp[PROGTP_CONFIG_TIMESTAMP_SIZE];
    ProgTP_ConfigOpType op_type;
    char description[PROGTP_CONFIG_DESCRIPTION_SIZE];
    ProgTP_Equipment before;
    ProgTP_Equipment after;
    ProgTP_ConfigEntryState entry_state;
} ProgTP_ConfigEntry;

typedef struct ProgTP_ConfigNode {
    ProgTP_ConfigEntry entry;
    struct ProgTP_ConfigNode *next;
} ProgTP_ConfigNode;

typedef struct {
    ProgTP_ConfigNode *top;
    size_t length;
    uint32_t next_id;
    size_t undo_count;
} ProgTP_ConfigHistory;

void ProgTP_ConfigHistoryInit(ProgTP_ConfigHistory *history);
void ProgTP_ConfigHistoryDestroy(ProgTP_ConfigHistory *history);
void ProgTP_ConfigHistoryClear(ProgTP_ConfigHistory *history);
bool ProgTP_ConfigHistoryCopy(ProgTP_ConfigHistory *destination, const ProgTP_ConfigHistory *source, char *error, size_t error_size);

size_t ProgTP_ConfigHistoryGetCount(const ProgTP_ConfigHistory *history);
const ProgTP_ConfigEntry *ProgTP_ConfigHistoryGetByIndex(const ProgTP_ConfigHistory *history, size_t index);
ProgTP_ConfigEntry *ProgTP_ConfigHistoryGetByIndexMut(ProgTP_ConfigHistory *history, size_t index);

bool ProgTP_ConfigHistoryLoad(ProgTP_ConfigHistory *history, const char *path, char *error, size_t error_size);
bool ProgTP_ConfigHistorySave(const ProgTP_ConfigHistory *history, const char *path, char *error, size_t error_size);

bool ProgTP_ConfigHistoryPush(
    ProgTP_ConfigHistory *history,
    ProgTP_ConfigOpType op_type,
    const ProgTP_Equipment *before,
    const ProgTP_Equipment *after,
    const char *description,
    char *error,
    size_t error_size);

bool ProgTP_ConfigHistoryUndo(
    ProgTP_ConfigHistory *history,
    ProgTP_EquipmentInventory *inventory,
    char *error,
    size_t error_size);

bool ProgTP_ConfigHistoryRedo(
    ProgTP_ConfigHistory *history,
    ProgTP_EquipmentInventory *inventory,
    char *error,
    size_t error_size);

bool ProgTP_ConfigHistoryPop(
    ProgTP_ConfigHistory *history,
    uint32_t id,
    char *error,
    size_t error_size);

bool ProgTP_ConfigHistoryImportFromFile(
    ProgTP_ConfigHistory *history,
    const char *path,
    char *error,
    size_t error_size);

bool ProgTP_ConfigHistoryCanUndo(const ProgTP_ConfigHistory *history);
bool ProgTP_ConfigHistoryCanRedo(const ProgTP_ConfigHistory *history);
size_t ProgTP_ConfigHistoryAppliedCount(const ProgTP_ConfigHistory *history);
size_t ProgTP_ConfigHistoryUndoneCount(const ProgTP_ConfigHistory *history);

const char *ProgTP_ConfigOpName(ProgTP_ConfigOpType op_type);
const char *ProgTP_ConfigEntryStateName(ProgTP_ConfigEntryState state);
void ProgTP_ConfigHistoryFormatRow(const ProgTP_ConfigEntry *entry, char *buffer, size_t buffer_size);
void ProgTP_ConfigHistoryFormatDetail(const ProgTP_ConfigEntry *entry, char *buffer, size_t buffer_size);

bool ProgTP_ConfigHistoryApplySnapshot(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_Equipment *snapshot,
    char *error,
    size_t error_size);

#endif
