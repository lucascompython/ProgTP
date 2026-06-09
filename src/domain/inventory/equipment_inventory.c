#include "equipment_inventory.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <string.h>
#define PROGTP_STRCASECMP _stricmp
#else
#include <strings.h>
#define PROGTP_STRCASECMP strcasecmp
#endif
#include <time.h>

#define PROGTP_EQUIPMENT_FILE_MAGIC "PTPEQP1"
#define PROGTP_EQUIPMENT_FILE_VERSION 1u

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t next_code;
    uint64_t count;
} ProgTP_EquipmentFileHeader;

static void SetError(char *error, size_t error_size, const char *message) {
    if (error_size == 0) {
        return;
    }
    snprintf(error, error_size, "%s", message ? message : "unknown error");
}

static void CopyText(char *destination, size_t destination_size, const char *source) {
    if (destination_size == 0) {
        return;
    }
    snprintf(destination, destination_size, "%s", source ? source : "");
}

bool ProgTP_TextEqualsIgnoreCase(const char *left, const char *right) {
    if (!left || !right) {
        return false;
    }
    return PROGTP_STRCASECMP(left, right) == 0;
}

static bool TextIsEmpty(const char *value) {
    return !value || value[0] == '\0';
}

static bool EnsureArrayCapacity(ProgTP_EquipmentArray *array, size_t required) {
    if (array->capacity >= required) {
        return true;
    }
    size_t next_capacity = array->capacity == 0 ? 8u : array->capacity * 2u;
    while (next_capacity < required) {
        next_capacity *= 2u;
    }
    ProgTP_Equipment *next = realloc(array->items, next_capacity * sizeof(*array->items));
    if (!next) {
        return false;
    }
    array->items = next;
    array->capacity = next_capacity;
    return true;
}

static size_t FindIndexByCode(const ProgTP_EquipmentInventory *inventory, uint32_t code) {
    for (size_t i = 0; i < inventory->array.length; ++i) {
        if (inventory->array.items[i].code == code) {
            return i;
        }
    }
    return (size_t)-1;
}

static bool ValidateInput(const ProgTP_EquipmentInventory *inventory, const ProgTP_EquipmentInput *input, uint32_t existing_code, char *error, size_t error_size) {
    if (!input || TextIsEmpty(input->name) || TextIsEmpty(input->type) || TextIsEmpty(input->ip_address) || TextIsEmpty(input->mac_address)) {
        SetError(error, error_size, "name, type, IP address, and MAC address are required");
        return false;
    }
    for (size_t i = 0; i < inventory->array.length; ++i) {
        const ProgTP_Equipment *equipment = &inventory->array.items[i];
        if (equipment->code == existing_code) {
            continue;
        }
        if (strcmp(equipment->ip_address, input->ip_address) == 0) {
            SetError(error, error_size, "another equipment already uses this IP address");
            return false;
        }
        if (ProgTP_TextEqualsIgnoreCase(equipment->mac_address, input->mac_address)) {
            SetError(error, error_size, "another equipment already uses this MAC address");
            return false;
        }
    }
    return true;
}

void ProgTP_EquipmentInventoryInit(ProgTP_EquipmentInventory *inventory) {
    memset(inventory, 0, sizeof(*inventory));
    inventory->next_code = 1;
}

void ProgTP_EquipmentInventoryDestroy(ProgTP_EquipmentInventory *inventory) {
    ProgTP_EquipmentInventoryClear(inventory);
}

void ProgTP_EquipmentInventoryClear(ProgTP_EquipmentInventory *inventory) {
    free(inventory->array.items);
    inventory->array.items = NULL;
    inventory->array.length = 0;
    inventory->array.capacity = 0;
    inventory->next_code = 1;
}

bool ProgTP_EquipmentInputInit(
    ProgTP_EquipmentInput *input,
    const char *name,
    const char *type,
    const char *brand,
    const char *model,
    const char *ip_address,
    const char *mac_address,
    const char *location,
    ProgTP_EquipmentState state) {
    if (!input) {
        return false;
    }
    memset(input, 0, sizeof(*input));
    CopyText(input->name, sizeof(input->name), name);
    CopyText(input->type, sizeof(input->type), type);
    CopyText(input->brand, sizeof(input->brand), brand);
    CopyText(input->model, sizeof(input->model), model);
    CopyText(input->ip_address, sizeof(input->ip_address), ip_address);
    CopyText(input->mac_address, sizeof(input->mac_address), mac_address);
    CopyText(input->location, sizeof(input->location), location);
    input->state = state;
    ProgTP_CurrentDateString(input->last_checked, sizeof(input->last_checked));
    return true;
}

bool ProgTP_EquipmentInventoryAdd(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_EquipmentInput *input,
    ProgTP_Equipment *created,
    char *error,
    size_t error_size) {
    if (!ValidateInput(inventory, input, 0, error, error_size)) {
        return false;
    }
    if (!EnsureArrayCapacity(&inventory->array, inventory->array.length + 1u)) {
        SetError(error, error_size, "not enough memory to add equipment");
        return false;
    }

    ProgTP_Equipment equipment = {0};
    equipment.code = inventory->next_code++;
    CopyText(equipment.name, sizeof(equipment.name), input->name);
    CopyText(equipment.type, sizeof(equipment.type), input->type);
    CopyText(equipment.brand, sizeof(equipment.brand), input->brand);
    CopyText(equipment.model, sizeof(equipment.model), input->model);
    CopyText(equipment.ip_address, sizeof(equipment.ip_address), input->ip_address);
    CopyText(equipment.mac_address, sizeof(equipment.mac_address), input->mac_address);
    CopyText(equipment.location, sizeof(equipment.location), input->location);
    equipment.state = input->state;
    CopyText(equipment.last_checked, sizeof(equipment.last_checked), TextIsEmpty(input->last_checked) ? "" : input->last_checked);
    if (TextIsEmpty(equipment.last_checked)) {
        ProgTP_CurrentDateString(equipment.last_checked, sizeof(equipment.last_checked));
    }

    inventory->array.items[inventory->array.length++] = equipment;
    if (created) {
        *created = equipment;
    }
    return true;
}

bool ProgTP_EquipmentInventoryRemove(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    char *error,
    size_t error_size) {
    size_t index = FindIndexByCode(inventory, code);
    if (index == (size_t)-1) {
        SetError(error, error_size, "equipment code not found");
        return false;
    }
    if (inventory->array.items[index].has_pending_incidents) {
        SetError(error, error_size, "equipment cannot be removed because it has pending technical incidents");
        return false;
    }
    if (index + 1u < inventory->array.length) {
        memmove(&inventory->array.items[index], &inventory->array.items[index + 1u], (inventory->array.length - index - 1u) * sizeof(*inventory->array.items));
    }
    --inventory->array.length;
    return true;
}

bool ProgTP_EquipmentInventoryUpdate(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    const ProgTP_EquipmentInput *input,
    char *error,
    size_t error_size) {
    size_t index = FindIndexByCode(inventory, code);
    if (index == (size_t)-1) {
        SetError(error, error_size, "equipment code not found");
        return false;
    }
    if (!ValidateInput(inventory, input, code, error, error_size)) {
        return false;
    }
    ProgTP_Equipment *equipment = &inventory->array.items[index];
    CopyText(equipment->name, sizeof(equipment->name), input->name);
    CopyText(equipment->type, sizeof(equipment->type), input->type);
    CopyText(equipment->brand, sizeof(equipment->brand), input->brand);
    CopyText(equipment->model, sizeof(equipment->model), input->model);
    CopyText(equipment->ip_address, sizeof(equipment->ip_address), input->ip_address);
    CopyText(equipment->mac_address, sizeof(equipment->mac_address), input->mac_address);
    CopyText(equipment->location, sizeof(equipment->location), input->location);
    equipment->state = input->state;
    CopyText(equipment->last_checked, sizeof(equipment->last_checked), input->last_checked);
    return true;
}

bool ProgTP_EquipmentInventorySetState(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    ProgTP_EquipmentState state,
    char *error,
    size_t error_size) {
    ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryFindByCode(inventory, code);
    if (!equipment) {
        SetError(error, error_size, "equipment code not found");
        return false;
    }
    equipment->state = state;
    ProgTP_CurrentDateString(equipment->last_checked, sizeof(equipment->last_checked));
    return true;
}

bool ProgTP_EquipmentInventorySetPendingIncidents(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    bool has_pending_incidents,
    char *error,
    size_t error_size) {
    ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryFindByCode(inventory, code);
    if (!equipment) {
        SetError(error, error_size, "equipment code not found");
        return false;
    }
    equipment->has_pending_incidents = has_pending_incidents;
    return true;
}

ProgTP_Equipment *ProgTP_EquipmentInventoryFindByCode(ProgTP_EquipmentInventory *inventory, uint32_t code) {
    size_t index = FindIndexByCode(inventory, code);
    return index == (size_t)-1 ? NULL : &inventory->array.items[index];
}

ProgTP_Equipment *ProgTP_EquipmentInventoryFindByIp(ProgTP_EquipmentInventory *inventory, const char *ip_address) {
    for (size_t i = 0; i < inventory->array.length; ++i) {
        if (strcmp(inventory->array.items[i].ip_address, ip_address) == 0) {
            return &inventory->array.items[i];
        }
    }
    return NULL;
}

ProgTP_Equipment *ProgTP_EquipmentInventoryFindByMac(ProgTP_EquipmentInventory *inventory, const char *mac_address) {
    for (size_t i = 0; i < inventory->array.length; ++i) {
        if (ProgTP_TextEqualsIgnoreCase(inventory->array.items[i].mac_address, mac_address)) {
            return &inventory->array.items[i];
        }
    }
    return NULL;
}

const ProgTP_Equipment *ProgTP_EquipmentInventoryFindByCodeConst(const ProgTP_EquipmentInventory *inventory, uint32_t code) {
    size_t index = FindIndexByCode(inventory, code);
    return index == (size_t)-1 ? NULL : &inventory->array.items[index];
}

void ProgTP_EquipmentInventoryVisitArray(const ProgTP_EquipmentInventory *inventory, ProgTP_EquipmentVisitor visitor, void *user_data) {
    for (size_t i = 0; i < inventory->array.length; ++i) {
        if (!visitor(&inventory->array.items[i], user_data)) {
            return;
        }
    }
}

void ProgTP_EquipmentInventoryVisitByType(const ProgTP_EquipmentInventory *inventory, const char *type, ProgTP_EquipmentVisitor visitor, void *user_data) {
    for (size_t i = 0; i < inventory->array.length; ++i) {
        if (ProgTP_TextEqualsIgnoreCase(inventory->array.items[i].type, type) && !visitor(&inventory->array.items[i], user_data)) {
            return;
        }
    }
}

void ProgTP_EquipmentInventoryVisitByState(const ProgTP_EquipmentInventory *inventory, ProgTP_EquipmentState state, ProgTP_EquipmentVisitor visitor, void *user_data) {
    for (size_t i = 0; i < inventory->array.length; ++i) {
        if (inventory->array.items[i].state == state && !visitor(&inventory->array.items[i], user_data)) {
            return;
        }
    }
}

bool ProgTP_EquipmentInventorySaveBinary(const ProgTP_EquipmentInventory *inventory, const char *path, char *error, size_t error_size) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        snprintf(error, error_size, "cannot open %s for writing: %s", path, strerror(errno));
        return false;
    }
    ProgTP_EquipmentFileHeader header = {0};
    memcpy(header.magic, PROGTP_EQUIPMENT_FILE_MAGIC, sizeof(header.magic) - 1u);
    header.version = PROGTP_EQUIPMENT_FILE_VERSION;
    header.next_code = inventory->next_code;
    header.count = (uint64_t)inventory->array.length;
    bool ok = fwrite(&header, sizeof(header), 1u, file) == 1u;
    if (ok && inventory->array.length > 0) {
        ok = fwrite(inventory->array.items, sizeof(*inventory->array.items), inventory->array.length, file) == inventory->array.length;
    }
    if (fclose(file) != 0) {
        ok = false;
    }
    if (!ok) {
        snprintf(error, error_size, "failed to write %s", path);
    }
    return ok;
}

bool ProgTP_EquipmentInventoryLoadBinary(ProgTP_EquipmentInventory *inventory, const char *path, char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, error_size, "cannot open %s for reading: %s", path, strerror(errno));
        return false;
    }
    ProgTP_EquipmentFileHeader header = {0};
    if (fread(&header, sizeof(header), 1u, file) != 1u ||
        memcmp(header.magic, PROGTP_EQUIPMENT_FILE_MAGIC, sizeof(header.magic) - 1u) != 0 ||
        header.version != PROGTP_EQUIPMENT_FILE_VERSION) {
        fclose(file);
        SetError(error, error_size, "invalid equipment inventory file");
        return false;
    }

    ProgTP_EquipmentInventory loaded;
    ProgTP_EquipmentInventoryInit(&loaded);
    loaded.next_code = header.next_code == 0 ? 1u : header.next_code;
    if (header.count > 0) {
        if (header.count > (uint64_t)(SIZE_MAX / sizeof(ProgTP_Equipment)) ||
            !EnsureArrayCapacity(&loaded.array, (size_t)header.count)) {
            fclose(file);
            ProgTP_EquipmentInventoryDestroy(&loaded);
            SetError(error, error_size, "not enough memory to load equipment inventory");
            return false;
        }
        loaded.array.length = (size_t)header.count;
        if (fread(loaded.array.items, sizeof(*loaded.array.items), loaded.array.length, file) != loaded.array.length) {
            fclose(file);
            ProgTP_EquipmentInventoryDestroy(&loaded);
            SetError(error, error_size, "failed to read equipment inventory records");
            return false;
        }
    }
    fclose(file);
    ProgTP_EquipmentInventoryClear(inventory);
    *inventory = loaded;
    return true;
}

bool ProgTP_EquipmentInventoryReplace(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_Equipment *items,
    size_t count,
    uint32_t next_code,
    char *error,
    size_t error_size) {
    if (count > 0 && !items) {
        SetError(error, error_size, "missing equipment records");
        return false;
    }

    uint32_t max_code = 0;
    for (size_t i = 0; i < count; ++i) {
        const ProgTP_Equipment *equipment = &items[i];
        if (equipment->code == 0 ||
            TextIsEmpty(equipment->name) ||
            TextIsEmpty(equipment->type) ||
            TextIsEmpty(equipment->ip_address) ||
            TextIsEmpty(equipment->mac_address)) {
            SetError(error, error_size, "loaded equipment has missing required fields");
            return false;
        }
        if ((int)equipment->state < (int)PROGTP_EQUIPMENT_OPERATIONAL ||
            (int)equipment->state > (int)PROGTP_EQUIPMENT_DISABLED) {
            SetError(error, error_size, "loaded equipment has invalid state");
            return false;
        }
        for (size_t j = i + 1u; j < count; ++j) {
            if (equipment->code == items[j].code) {
                SetError(error, error_size, "loaded equipment contains duplicate codes");
                return false;
            }
            if (strcmp(equipment->ip_address, items[j].ip_address) == 0) {
                SetError(error, error_size, "loaded equipment contains duplicate IP addresses");
                return false;
            }
            if (ProgTP_TextEqualsIgnoreCase(equipment->mac_address, items[j].mac_address)) {
                SetError(error, error_size, "loaded equipment contains duplicate MAC addresses");
                return false;
            }
        }
        if (equipment->code > max_code) {
            max_code = equipment->code;
        }
    }

    ProgTP_EquipmentInventory loaded;
    ProgTP_EquipmentInventoryInit(&loaded);
    if (count > 0) {
        if (!EnsureArrayCapacity(&loaded.array, count)) {
            ProgTP_EquipmentInventoryDestroy(&loaded);
            SetError(error, error_size, "not enough memory to replace equipment inventory");
            return false;
        }
        memcpy(loaded.array.items, items, count * sizeof(*items));
        loaded.array.length = count;
    }
    loaded.next_code = next_code > max_code ? next_code : max_code + 1u;
    if (loaded.next_code == 0) {
        loaded.next_code = 1;
    }

    ProgTP_EquipmentInventoryClear(inventory);
    *inventory = loaded;
    return true;
}

void ProgTP_EquipmentInventorySeedDefaults(ProgTP_EquipmentInventory *inventory) {
    if (inventory->array.length > 0) {
        return;
    }
    const ProgTP_EquipmentInput defaults[] = {
        { "Core Router", "Router", "Cisco", "ISR 4331", "192.168.1.1", "00:11:22:33:44:01", "Main rack", PROGTP_EQUIPMENT_OPERATIONAL, "2026-05-28" },
        { "Access Switch A", "Switch", "HP", "Aruba 2530", "192.168.1.2", "00:11:22:33:44:02", "Main rack", PROGTP_EQUIPMENT_OPERATIONAL, "2026-05-28" },
        { "Office AP 1", "Access Point", "Ubiquiti", "U6 Lite", "192.168.1.10", "00:11:22:33:44:03", "Office ceiling", PROGTP_EQUIPMENT_MAINTENANCE, "2026-05-28" },
        { "NAS Backup", "NAS", "Synology", "DS923+", "192.168.1.20", "00:11:22:33:44:04", "Server room", PROGTP_EQUIPMENT_OPERATIONAL, "2026-05-28" },
        { "UPS Rack", "UPS", "APC", "Smart-UPS 1500", "192.168.1.30", "00:11:22:33:44:05", "Main rack", PROGTP_EQUIPMENT_OPERATIONAL, "2026-05-28" },
    };
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); ++i) {
        char ignored[128] = {0};
        ProgTP_EquipmentInventoryAdd(inventory, &defaults[i], NULL, ignored, sizeof(ignored));
    }
}

void ProgTP_EquipmentInventorySummary(const ProgTP_EquipmentInventory *inventory, char *buffer, size_t buffer_size) {
    size_t operational = 0;
    size_t failed = 0;
    size_t maintenance = 0;
    size_t disabled = 0;
    for (size_t i = 0; i < inventory->array.length; ++i) {
        switch (inventory->array.items[i].state) {
            case PROGTP_EQUIPMENT_OPERATIONAL: ++operational; break;
            case PROGTP_EQUIPMENT_FAILED: ++failed; break;
            case PROGTP_EQUIPMENT_MAINTENANCE: ++maintenance; break;
            case PROGTP_EQUIPMENT_DISABLED: ++disabled; break;
        }
    }
    snprintf(
        buffer,
        buffer_size,
        "Module 1 inventory: %zu equipment (%zu operational, %zu failed, %zu maintenance, %zu disabled)",
        inventory->array.length,
        operational,
        failed,
        maintenance,
        disabled);
}

void ProgTP_EquipmentFormatLine(const ProgTP_Equipment *equipment, char *buffer, size_t buffer_size) {
    snprintf(
        buffer,
        buffer_size,
        "#%u | %s | %s | %s %s | IP %s | MAC %s | %s | %s | last check %s%s",
        equipment->code,
        equipment->name,
        equipment->type,
        equipment->brand,
        equipment->model,
        equipment->ip_address,
        equipment->mac_address,
        equipment->location,
        ProgTP_EquipmentStateName(equipment->state),
        equipment->last_checked,
        equipment->has_pending_incidents ? " | pending incidents" : "");
}

const char *ProgTP_EquipmentStateName(ProgTP_EquipmentState state) {
    switch (state) {
        case PROGTP_EQUIPMENT_OPERATIONAL: return "Operational";
        case PROGTP_EQUIPMENT_FAILED: return "Failed";
        case PROGTP_EQUIPMENT_MAINTENANCE: return "Maintenance";
        case PROGTP_EQUIPMENT_DISABLED: return "Disabled";
    }
    return "Unknown";
}

bool ProgTP_EquipmentStateFromString(const char *value, ProgTP_EquipmentState *state) {
    if (ProgTP_TextEqualsIgnoreCase(value, "operational")) {
        *state = PROGTP_EQUIPMENT_OPERATIONAL;
        return true;
    }
    if (ProgTP_TextEqualsIgnoreCase(value, "failed")) {
        *state = PROGTP_EQUIPMENT_FAILED;
        return true;
    }
    if (ProgTP_TextEqualsIgnoreCase(value, "maintenance")) {
        *state = PROGTP_EQUIPMENT_MAINTENANCE;
        return true;
    }
    if (ProgTP_TextEqualsIgnoreCase(value, "disabled")) {
        *state = PROGTP_EQUIPMENT_DISABLED;
        return true;
    }
    return false;
}

void ProgTP_CurrentDateString(char *buffer, size_t buffer_size) {
#if defined(PROGTP_WEB)
    CopyText(buffer, buffer_size, "2026-05-28");
#else
    time_t now = time(NULL);
    struct tm local_time;
#if defined(_WIN32)
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif
    strftime(buffer, buffer_size, "%Y-%m-%d", &local_time);
#endif
}
