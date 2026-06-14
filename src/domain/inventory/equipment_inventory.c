#include "equipment_inventory.h"

#include "progtp_error.h"
#include "progtp_text.h"
#include "progtp_time.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGTP_EQUIPMENT_FILE_MAGIC "PTPEQP1"
#define PROGTP_EQUIPMENT_FILE_VERSION 1u

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t next_code;
    uint64_t count;
} ProgTP_EquipmentFileHeader;

static ProgTP_EquipmentNode *FindNodeByCode(ProgTP_EquipmentInventory *inventory, uint32_t code) {
    ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (current->equipment.code == code) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static const ProgTP_EquipmentNode *FindNodeByCodeConst(const ProgTP_EquipmentInventory *inventory, uint32_t code) {
    const ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (current->equipment.code == code) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static bool ValidateInput(const ProgTP_EquipmentInventory *inventory, const ProgTP_EquipmentInput *input, uint32_t existing_code, char *error, size_t error_size) {
    if (!input || ProgTP_TextIsEmpty(input->name) || ProgTP_TextIsEmpty(input->type) || ProgTP_TextIsEmpty(input->ip_address) || ProgTP_TextIsEmpty(input->mac_address)) {
        ProgTP_SetError(error, error_size, "name, type, IP address, and MAC address are required");
        return false;
    }
    const ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (current->equipment.code == existing_code) {
            current = current->next;
            continue;
        }
        if (strcmp(current->equipment.ip_address, input->ip_address) == 0) {
            ProgTP_SetError(error, error_size, "another equipment already uses this IP address");
            return false;
        }
        if (ProgTP_TextEqualsIgnoreCase(current->equipment.mac_address, input->mac_address)) {
            ProgTP_SetError(error, error_size, "another equipment already uses this MAC address");
            return false;
        }
        current = current->next;
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
    ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        ProgTP_EquipmentNode *next = current->next;
        free(current);
        current = next;
    }
    inventory->head = NULL;
    inventory->tail = NULL;
    inventory->length = 0;
    inventory->next_code = 1;
}

size_t ProgTP_EquipmentInventoryGetCount(const ProgTP_EquipmentInventory *inventory) {
    return inventory ? inventory->length : 0u;
}

const ProgTP_Equipment *ProgTP_EquipmentInventoryGetByIndex(const ProgTP_EquipmentInventory *inventory, size_t index) {
    if (!inventory || index >= inventory->length) {
        return NULL;
    }
    const ProgTP_EquipmentNode *current = inventory->head;
    for (size_t i = 0; i < index && current; ++i) {
        current = current->next;
    }
    return current ? &current->equipment : NULL;
}

ProgTP_Equipment *ProgTP_EquipmentInventoryGetByIndexMut(ProgTP_EquipmentInventory *inventory, size_t index) {
    if (!inventory || index >= inventory->length) {
        return NULL;
    }
    ProgTP_EquipmentNode *current = inventory->head;
    for (size_t i = 0; i < index && current; ++i) {
        current = current->next;
    }
    return current ? &current->equipment : NULL;
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
    ProgTP_TextCopy(input->name, sizeof(input->name), name);
    ProgTP_TextCopy(input->type, sizeof(input->type), type);
    ProgTP_TextCopy(input->brand, sizeof(input->brand), brand);
    ProgTP_TextCopy(input->model, sizeof(input->model), model);
    ProgTP_TextCopy(input->ip_address, sizeof(input->ip_address), ip_address);
    ProgTP_TextCopy(input->mac_address, sizeof(input->mac_address), mac_address);
    ProgTP_TextCopy(input->location, sizeof(input->location), location);
    input->state = state;
    ProgTP_FormatCurrentDate(input->last_checked, sizeof(input->last_checked));
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
    ProgTP_EquipmentNode *node = calloc(1u, sizeof(*node));
    if (!node) {
        ProgTP_SetError(error, error_size, "not enough memory to add equipment");
        return false;
    }
    node->equipment.code = inventory->next_code++;
    ProgTP_TextCopy(node->equipment.name, sizeof(node->equipment.name), input->name);
    ProgTP_TextCopy(node->equipment.type, sizeof(node->equipment.type), input->type);
    ProgTP_TextCopy(node->equipment.brand, sizeof(node->equipment.brand), input->brand);
    ProgTP_TextCopy(node->equipment.model, sizeof(node->equipment.model), input->model);
    ProgTP_TextCopy(node->equipment.ip_address, sizeof(node->equipment.ip_address), input->ip_address);
    ProgTP_TextCopy(node->equipment.mac_address, sizeof(node->equipment.mac_address), input->mac_address);
    ProgTP_TextCopy(node->equipment.location, sizeof(node->equipment.location), input->location);
    node->equipment.state = input->state;
    ProgTP_TextCopy(node->equipment.last_checked, sizeof(node->equipment.last_checked), ProgTP_TextIsEmpty(input->last_checked) ? "" : input->last_checked);
    if (ProgTP_TextIsEmpty(node->equipment.last_checked)) {
        ProgTP_FormatCurrentDate(node->equipment.last_checked, sizeof(node->equipment.last_checked));
    }
    node->next = NULL;

    if (!inventory->tail) {
        inventory->head = node;
        inventory->tail = node;
    } else {
        inventory->tail->next = node;
        inventory->tail = node;
    }
    ++inventory->length;

    if (created) {
        *created = node->equipment;
    }
    return true;
}

bool ProgTP_EquipmentInventoryRemove(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    char *error,
    size_t error_size) {
    ProgTP_EquipmentNode *prev = NULL;
    ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (current->equipment.code == code) {
            break;
        }
        prev = current;
        current = current->next;
    }
    if (!current) {
        ProgTP_SetError(error, error_size, "equipment code not found");
        return false;
    }
    if (current->equipment.has_pending_incidents) {
        ProgTP_SetError(error, error_size, "equipment cannot be removed because it has pending technical incidents");
        return false;
    }
    if (prev) {
        prev->next = current->next;
    } else {
        inventory->head = current->next;
    }
    if (current == inventory->tail) {
        inventory->tail = prev;
    }
    free(current);
    --inventory->length;
    return true;
}

bool ProgTP_EquipmentInventoryUpdate(
    ProgTP_EquipmentInventory *inventory,
    uint32_t code,
    const ProgTP_EquipmentInput *input,
    char *error,
    size_t error_size) {
    ProgTP_EquipmentNode *node = FindNodeByCode(inventory, code);
    if (!node) {
        ProgTP_SetError(error, error_size, "equipment code not found");
        return false;
    }
    if (!ValidateInput(inventory, input, code, error, error_size)) {
        return false;
    }
    ProgTP_Equipment *equipment = &node->equipment;
    ProgTP_TextCopy(equipment->name, sizeof(equipment->name), input->name);
    ProgTP_TextCopy(equipment->type, sizeof(equipment->type), input->type);
    ProgTP_TextCopy(equipment->brand, sizeof(equipment->brand), input->brand);
    ProgTP_TextCopy(equipment->model, sizeof(equipment->model), input->model);
    ProgTP_TextCopy(equipment->ip_address, sizeof(equipment->ip_address), input->ip_address);
    ProgTP_TextCopy(equipment->mac_address, sizeof(equipment->mac_address), input->mac_address);
    ProgTP_TextCopy(equipment->location, sizeof(equipment->location), input->location);
    equipment->state = input->state;
    ProgTP_TextCopy(equipment->last_checked, sizeof(equipment->last_checked), input->last_checked);
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
        ProgTP_SetError(error, error_size, "equipment code not found");
        return false;
    }
    equipment->state = state;
    ProgTP_FormatCurrentDate(equipment->last_checked, sizeof(equipment->last_checked));
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
        ProgTP_SetError(error, error_size, "equipment code not found");
        return false;
    }
    equipment->has_pending_incidents = has_pending_incidents;
    return true;
}

ProgTP_Equipment *ProgTP_EquipmentInventoryFindByCode(ProgTP_EquipmentInventory *inventory, uint32_t code) {
    ProgTP_EquipmentNode *node = FindNodeByCode(inventory, code);
    return node ? &node->equipment : NULL;
}

ProgTP_Equipment *ProgTP_EquipmentInventoryFindByIp(ProgTP_EquipmentInventory *inventory, const char *ip_address) {
    ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (strcmp(current->equipment.ip_address, ip_address) == 0) {
            return &current->equipment;
        }
        current = current->next;
    }
    return NULL;
}

ProgTP_Equipment *ProgTP_EquipmentInventoryFindByMac(ProgTP_EquipmentInventory *inventory, const char *mac_address) {
    ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (ProgTP_TextEqualsIgnoreCase(current->equipment.mac_address, mac_address)) {
            return &current->equipment;
        }
        current = current->next;
    }
    return NULL;
}

const ProgTP_Equipment *ProgTP_EquipmentInventoryFindByCodeConst(const ProgTP_EquipmentInventory *inventory, uint32_t code) {
    const ProgTP_EquipmentNode *node = FindNodeByCodeConst(inventory, code);
    return node ? &node->equipment : NULL;
}

void ProgTP_EquipmentInventoryForEach(const ProgTP_EquipmentInventory *inventory, ProgTP_EquipmentVisitor visitor, void *user_data) {
    const ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (!visitor(&current->equipment, user_data)) {
            return;
        }
        current = current->next;
    }
}

void ProgTP_EquipmentInventoryForEachByType(const ProgTP_EquipmentInventory *inventory, const char *type, ProgTP_EquipmentVisitor visitor, void *user_data) {
    const ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (ProgTP_TextEqualsIgnoreCase(current->equipment.type, type) && !visitor(&current->equipment, user_data)) {
            return;
        }
        current = current->next;
    }
}

void ProgTP_EquipmentInventoryForEachByState(const ProgTP_EquipmentInventory *inventory, ProgTP_EquipmentState state, ProgTP_EquipmentVisitor visitor, void *user_data) {
    const ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        if (current->equipment.state == state && !visitor(&current->equipment, user_data)) {
            return;
        }
        current = current->next;
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
    header.count = (uint64_t)inventory->length;

    bool ok = fwrite(&header, sizeof(header), 1u, file) == 1u;
    if (ok) {
        const ProgTP_EquipmentNode *current = inventory->head;
        while (current) {
            if (fwrite(&current->equipment, sizeof(current->equipment), 1u, file) != 1u) {
                ok = false;
                break;
            }
            current = current->next;
        }
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
        ProgTP_SetError(error, error_size, "invalid equipment inventory file");
        return false;
    }

    ProgTP_EquipmentInventory loaded;
    ProgTP_EquipmentInventoryInit(&loaded);
    loaded.next_code = header.next_code == 0 ? 1u : header.next_code;

    ProgTP_EquipmentNode **tail_ptr = &loaded.head;
    for (uint64_t i = 0; i < header.count; ++i) {
        ProgTP_EquipmentNode *node = calloc(1u, sizeof(*node));
        if (!node || fread(&node->equipment, sizeof(node->equipment), 1u, file) != 1u) {
            free(node);
            fclose(file);
            ProgTP_EquipmentInventoryDestroy(&loaded);
            ProgTP_SetError(error, error_size, "failed to read equipment inventory records");
            return false;
        }
        node->next = NULL;
        node->equipment.name[sizeof(node->equipment.name) - 1u] = '\0';
        node->equipment.type[sizeof(node->equipment.type) - 1u] = '\0';
        node->equipment.brand[sizeof(node->equipment.brand) - 1u] = '\0';
        node->equipment.model[sizeof(node->equipment.model) - 1u] = '\0';
        node->equipment.ip_address[sizeof(node->equipment.ip_address) - 1u] = '\0';
        node->equipment.mac_address[sizeof(node->equipment.mac_address) - 1u] = '\0';
        node->equipment.location[sizeof(node->equipment.location) - 1u] = '\0';
        node->equipment.last_checked[sizeof(node->equipment.last_checked) - 1u] = '\0';
        *tail_ptr = node;
        tail_ptr = &node->next;
        loaded.tail = node;
    }
    loaded.length = (size_t)header.count;
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
        ProgTP_SetError(error, error_size, "missing equipment records");
        return false;
    }

    uint32_t max_code = 0;
    for (size_t i = 0; i < count; ++i) {
        const ProgTP_Equipment *equipment = &items[i];
        if (equipment->code == 0 ||
            ProgTP_TextIsEmpty(equipment->name) ||
            ProgTP_TextIsEmpty(equipment->type) ||
            ProgTP_TextIsEmpty(equipment->ip_address) ||
            ProgTP_TextIsEmpty(equipment->mac_address)) {
            ProgTP_SetError(error, error_size, "loaded equipment has missing required fields");
            return false;
        }
        if ((int)equipment->state < (int)PROGTP_EQUIPMENT_OPERATIONAL ||
            (int)equipment->state > (int)PROGTP_EQUIPMENT_DISABLED) {
            ProgTP_SetError(error, error_size, "loaded equipment has invalid state");
            return false;
        }
        for (size_t j = i + 1u; j < count; ++j) {
            if (equipment->code == items[j].code) {
                ProgTP_SetError(error, error_size, "loaded equipment contains duplicate codes");
                return false;
            }
            if (strcmp(equipment->ip_address, items[j].ip_address) == 0) {
                ProgTP_SetError(error, error_size, "loaded equipment contains duplicate IP addresses");
                return false;
            }
            if (ProgTP_TextEqualsIgnoreCase(equipment->mac_address, items[j].mac_address)) {
                ProgTP_SetError(error, error_size, "loaded equipment contains duplicate MAC addresses");
                return false;
            }
        }
        if (equipment->code > max_code) {
            max_code = equipment->code;
        }
    }

    ProgTP_EquipmentInventory loaded;
    ProgTP_EquipmentInventoryInit(&loaded);

    ProgTP_EquipmentNode **tail_ptr = &loaded.head;
    for (size_t i = 0; i < count; ++i) {
        ProgTP_EquipmentNode *node = calloc(1u, sizeof(*node));
        if (!node) {
            ProgTP_EquipmentInventoryDestroy(&loaded);
            ProgTP_SetError(error, error_size, "not enough memory to replace equipment inventory");
            return false;
        }
        node->equipment = items[i];
        node->next = NULL;
        *tail_ptr = node;
        tail_ptr = &node->next;
        loaded.tail = node;
    }
    loaded.length = count;
    loaded.next_code = next_code > max_code ? next_code : max_code + 1u;
    if (loaded.next_code == 0) {
        loaded.next_code = 1;
    }

    ProgTP_EquipmentInventoryClear(inventory);
    *inventory = loaded;
    return true;
}

bool ProgTP_EquipmentInventoryApplySnapshot(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_Equipment *snapshot,
    char *error,
    size_t error_size) {
    if (!inventory || !snapshot) {
        ProgTP_SetError(error, error_size, "missing equipment inventory or snapshot");
        return false;
    }
    if (snapshot->code == 0) {
        ProgTP_SetError(error, error_size, "snapshot has no equipment code");
        return false;
    }

    ProgTP_EquipmentNode *existing = FindNodeByCode(inventory, snapshot->code);
    if (existing) {
        existing->equipment = *snapshot;
    } else {
        ProgTP_EquipmentNode *node = calloc(1u, sizeof(*node));
        if (!node) {
            ProgTP_SetError(error, error_size, "not enough memory to restore equipment");
            return false;
        }
        node->equipment = *snapshot;
        node->next = NULL;
        if (!inventory->tail) {
            inventory->head = node;
            inventory->tail = node;
        } else {
            inventory->tail->next = node;
            inventory->tail = node;
        }
        ++inventory->length;
        if (inventory->next_code <= snapshot->code) {
            inventory->next_code = snapshot->code + 1u;
        }
    }
    return true;
}

void ProgTP_EquipmentInventorySeedDefaults(ProgTP_EquipmentInventory *inventory) {
    if (inventory->length > 0) {
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
    const ProgTP_EquipmentNode *current = inventory->head;
    while (current) {
        switch (current->equipment.state) {
            case PROGTP_EQUIPMENT_OPERATIONAL: ++operational; break;
            case PROGTP_EQUIPMENT_FAILED: ++failed; break;
            case PROGTP_EQUIPMENT_MAINTENANCE: ++maintenance; break;
            case PROGTP_EQUIPMENT_DISABLED: ++disabled; break;
        }
        current = current->next;
    }
    snprintf(
        buffer,
        buffer_size,
        "Module 1 inventory: %zu equipment (%zu operational, %zu failed, %zu maintenance, %zu disabled)",
        inventory->length,
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
