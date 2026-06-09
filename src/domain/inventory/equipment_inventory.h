#ifndef PROGTP_EQUIPMENT_INVENTORY_H
#define PROGTP_EQUIPMENT_INVENTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGTP_EQUIPMENT_NAME_SIZE 64u
#define PROGTP_EQUIPMENT_TYPE_SIZE 32u
#define PROGTP_EQUIPMENT_BRAND_SIZE 32u
#define PROGTP_EQUIPMENT_MODEL_SIZE 32u
#define PROGTP_EQUIPMENT_IP_SIZE 46u
#define PROGTP_EQUIPMENT_MAC_SIZE 32u
#define PROGTP_EQUIPMENT_LOCATION_SIZE 64u
#define PROGTP_EQUIPMENT_DATE_SIZE 20u

typedef enum {
    PROGTP_EQUIPMENT_OPERATIONAL = 0,
    PROGTP_EQUIPMENT_FAILED = 1,
    PROGTP_EQUIPMENT_MAINTENANCE = 2,
    PROGTP_EQUIPMENT_DISABLED = 3,
} ProgTP_EquipmentState;

typedef struct {
    uint32_t code;
    char name[PROGTP_EQUIPMENT_NAME_SIZE];
    char type[PROGTP_EQUIPMENT_TYPE_SIZE];
    char brand[PROGTP_EQUIPMENT_BRAND_SIZE];
    char model[PROGTP_EQUIPMENT_MODEL_SIZE];
    char ip_address[PROGTP_EQUIPMENT_IP_SIZE];
    char mac_address[PROGTP_EQUIPMENT_MAC_SIZE];
    char location[PROGTP_EQUIPMENT_LOCATION_SIZE];
    ProgTP_EquipmentState state;
    char last_checked[PROGTP_EQUIPMENT_DATE_SIZE];
    bool has_pending_incidents;
} ProgTP_Equipment;

typedef struct {
    char name[PROGTP_EQUIPMENT_NAME_SIZE];
    char type[PROGTP_EQUIPMENT_TYPE_SIZE];
    char brand[PROGTP_EQUIPMENT_BRAND_SIZE];
    char model[PROGTP_EQUIPMENT_MODEL_SIZE];
    char ip_address[PROGTP_EQUIPMENT_IP_SIZE];
    char mac_address[PROGTP_EQUIPMENT_MAC_SIZE];
    char location[PROGTP_EQUIPMENT_LOCATION_SIZE];
    ProgTP_EquipmentState state;
    char last_checked[PROGTP_EQUIPMENT_DATE_SIZE];
} ProgTP_EquipmentInput;

typedef struct {
    ProgTP_Equipment *items;
    size_t length;
    size_t capacity;
} ProgTP_EquipmentArray;

typedef struct {
    ProgTP_EquipmentArray array;
    uint32_t next_code;
} ProgTP_EquipmentInventory;

typedef bool (*ProgTP_EquipmentVisitor)(const ProgTP_Equipment *equipment, void *user_data);

void ProgTP_EquipmentInventoryInit(ProgTP_EquipmentInventory *inventory);
void ProgTP_EquipmentInventoryDestroy(ProgTP_EquipmentInventory *inventory);
void ProgTP_EquipmentInventoryClear(ProgTP_EquipmentInventory *inventory);

bool ProgTP_EquipmentInputInit(
    ProgTP_EquipmentInput *input,
    const char *name,
    const char *type,
    const char *brand,
    const char *model,
    const char *ip_address,
    const char *mac_address,
    const char *location,
    ProgTP_EquipmentState state);

bool ProgTP_EquipmentInventoryAdd(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_EquipmentInput *input,
    ProgTP_Equipment *created,
    char *error,
    size_t error_size);

bool ProgTP_EquipmentInventoryRemove(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    char *error,
    size_t error_size);

bool ProgTP_EquipmentInventoryUpdate(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    const ProgTP_EquipmentInput *input,
    char *error,
    size_t error_size);

bool ProgTP_EquipmentInventorySetState(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    ProgTP_EquipmentState state,
    char *error,
    size_t error_size);

bool ProgTP_EquipmentInventorySetPendingIncidents(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    bool has_pending_incidents,
    char *error,
    size_t error_size);

ProgTP_Equipment *ProgTP_EquipmentInventoryFindByCode(ProgTP_EquipmentInventory *inventory, uint32_t code);
ProgTP_Equipment *ProgTP_EquipmentInventoryFindByIp(ProgTP_EquipmentInventory *inventory, const char *ip_address);
ProgTP_Equipment *ProgTP_EquipmentInventoryFindByMac(ProgTP_EquipmentInventory *inventory, const char *mac_address);
const ProgTP_Equipment *ProgTP_EquipmentInventoryFindByCodeConst(const ProgTP_EquipmentInventory *inventory, uint32_t code);

void ProgTP_EquipmentInventoryVisitArray(const ProgTP_EquipmentInventory *inventory, ProgTP_EquipmentVisitor visitor, void *user_data);
void ProgTP_EquipmentInventoryVisitByType(const ProgTP_EquipmentInventory *inventory, const char *type, ProgTP_EquipmentVisitor visitor, void *user_data);
void ProgTP_EquipmentInventoryVisitByState(const ProgTP_EquipmentInventory *inventory, ProgTP_EquipmentState state, ProgTP_EquipmentVisitor visitor, void *user_data);

bool ProgTP_EquipmentInventorySaveBinary(const ProgTP_EquipmentInventory *inventory, const char *path, char *error, size_t error_size);
bool ProgTP_EquipmentInventoryLoadBinary(ProgTP_EquipmentInventory *inventory, const char *path, char *error, size_t error_size);
bool ProgTP_EquipmentInventoryReplace(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_Equipment *items,
    size_t count,
    uint32_t next_code,
    char *error,
    size_t error_size);

void ProgTP_EquipmentInventorySeedDefaults(ProgTP_EquipmentInventory *inventory);
void ProgTP_EquipmentInventorySummary(const ProgTP_EquipmentInventory *inventory, char *buffer, size_t buffer_size);
void ProgTP_EquipmentFormatLine(const ProgTP_Equipment *equipment, char *buffer, size_t buffer_size);
const char *ProgTP_EquipmentStateName(ProgTP_EquipmentState state);
bool ProgTP_EquipmentStateFromString(const char *value, ProgTP_EquipmentState *state);
void ProgTP_CurrentDateString(char *buffer, size_t buffer_size);
bool ProgTP_TextEqualsIgnoreCase(const char *left, const char *right);

#endif
