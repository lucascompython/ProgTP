#include "protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

static void CopyString(char *destination, size_t destination_size, const char *source) {
    if (destination_size == 0) {
        return;
    }
    snprintf(destination, destination_size, "%s", source ? source : "");
}

void ProgTP_RunLocalCommand(ProgTP_CommandResult *result) {
    ProgTP_EquipmentInventory inventory;
    ProgTP_EquipmentInventoryInit(&inventory);
    char error[256] = {0};
    if (!ProgTP_EquipmentInventoryLoadBinary(&inventory, "equipamentos.dat", error, sizeof(error))) {
        ProgTP_EquipmentInventorySeedDefaults(&inventory);
        ProgTP_EquipmentInventorySaveBinary(&inventory, "equipamentos.dat", error, sizeof(error));
    }
    ProgTP_EquipmentInventorySummary(&inventory, result->message, sizeof(result->message));
    ProgTP_EquipmentInventoryDestroy(&inventory);
    CopyString(result->mode, sizeof(result->mode), "local");
}

char *ProgTP_CommandResultToJson(const ProgTP_CommandResult *result, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "message", result->message);
    yyjson_mut_obj_add_str(doc, root, "mode", result->mode);

    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_CommandResultFromJson(const char *json, size_t json_length, ProgTP_CommandResult *result) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *message = yyjson_obj_get(root, "message");
    yyjson_val *mode = yyjson_obj_get(root, "mode");
    if (!yyjson_is_str(message) || !yyjson_is_str(mode)) {
        yyjson_doc_free(doc);
        return false;
    }

    CopyString(result->message, sizeof(result->message), yyjson_get_str(message));
    CopyString(result->mode, sizeof(result->mode), yyjson_get_str(mode));
    yyjson_doc_free(doc);
    return true;
}

void ProgTP_FormatCommandResultLabel(const ProgTP_CommandResult *result, char *buffer, size_t buffer_size) {
    if (result && strcmp(result->mode, "server") == 0) {
        snprintf(buffer, buffer_size, "Mode: connected to server | HTTP API online");
    } else {
        snprintf(buffer, buffer_size, "Mode: local | data stored on this computer");
    }
}

static void SetProtocolError(char *error, size_t error_size, const char *message) {
    if (error_size == 0) {
        return;
    }
    snprintf(error, error_size, "%s", message ? message : "invalid inventory JSON");
}

static bool ReadRequiredString(yyjson_val *object, const char *name, char *destination, size_t destination_size, char *error, size_t error_size) {
    yyjson_val *value = yyjson_obj_get(object, name);
    if (!yyjson_is_str(value)) {
        char message[96];
        snprintf(message, sizeof(message), "missing or invalid %s", name);
        SetProtocolError(error, error_size, message);
        return false;
    }
    CopyString(destination, destination_size, yyjson_get_str(value));
    return true;
}

static void ReadOptionalString(yyjson_val *object, const char *name, char *destination, size_t destination_size) {
    yyjson_val *value = yyjson_obj_get(object, name);
    CopyString(destination, destination_size, yyjson_is_str(value) ? yyjson_get_str(value) : "");
}

char *ProgTP_EquipmentInventoryToJson(const ProgTP_EquipmentInventory *inventory, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_uint(doc, root, "next_code", inventory->next_code);

    yyjson_mut_val *items = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "equipment", items);
    for (size_t i = 0; i < inventory->array.length; ++i) {
        const ProgTP_Equipment *equipment = &inventory->array.items[i];
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        yyjson_mut_arr_add_val(items, item);
        yyjson_mut_obj_add_uint(doc, item, "code", equipment->code);
        yyjson_mut_obj_add_str(doc, item, "name", equipment->name);
        yyjson_mut_obj_add_str(doc, item, "type", equipment->type);
        yyjson_mut_obj_add_str(doc, item, "brand", equipment->brand);
        yyjson_mut_obj_add_str(doc, item, "model", equipment->model);
        yyjson_mut_obj_add_str(doc, item, "ip_address", equipment->ip_address);
        yyjson_mut_obj_add_str(doc, item, "mac_address", equipment->mac_address);
        yyjson_mut_obj_add_str(doc, item, "location", equipment->location);
        yyjson_mut_obj_add_str(doc, item, "state", ProgTP_EquipmentStateName(equipment->state));
        yyjson_mut_obj_add_str(doc, item, "last_checked", equipment->last_checked);
        yyjson_mut_obj_add_bool(doc, item, "has_pending_incidents", equipment->has_pending_incidents);
    }

    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_EquipmentInventoryFromJson(
    const char *json,
    size_t json_length,
    ProgTP_EquipmentInventory *inventory,
    char *error,
    size_t error_size) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        SetProtocolError(error, error_size, "invalid inventory JSON");
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *next_code_value = yyjson_obj_get(root, "next_code");
    yyjson_val *items_value = yyjson_obj_get(root, "equipment");
    if (!yyjson_is_obj(root) || !yyjson_is_uint(next_code_value) || !yyjson_is_arr(items_value)) {
        yyjson_doc_free(doc);
        SetProtocolError(error, error_size, "inventory JSON must contain next_code and equipment");
        return false;
    }

    uint64_t next_code_u64 = yyjson_get_uint(next_code_value);
    if (next_code_u64 > UINT32_MAX) {
        yyjson_doc_free(doc);
        SetProtocolError(error, error_size, "next_code is too large");
        return false;
    }

    size_t item_count = yyjson_arr_size(items_value);
    ProgTP_Equipment *items = NULL;
    if (item_count > 0) {
        if (item_count > SIZE_MAX / sizeof(*items)) {
            yyjson_doc_free(doc);
            SetProtocolError(error, error_size, "inventory JSON is too large");
            return false;
        }
        items = calloc(item_count, sizeof(*items));
        if (!items) {
            yyjson_doc_free(doc);
            SetProtocolError(error, error_size, "not enough memory to parse inventory JSON");
            return false;
        }
    }

    size_t index = 0;
    yyjson_val *item = NULL;
    yyjson_arr_iter iter = yyjson_arr_iter_with(items_value);
    while ((item = yyjson_arr_iter_next(&iter)) != NULL) {
        if (!yyjson_is_obj(item)) {
            free(items);
            yyjson_doc_free(doc);
            SetProtocolError(error, error_size, "equipment entry must be an object");
            return false;
        }

        yyjson_val *code_value = yyjson_obj_get(item, "code");
        yyjson_val *state_value = yyjson_obj_get(item, "state");
        if (!yyjson_is_uint(code_value) || yyjson_get_uint(code_value) > UINT32_MAX || !yyjson_is_str(state_value)) {
            free(items);
            yyjson_doc_free(doc);
            SetProtocolError(error, error_size, "equipment entry has invalid code or state");
            return false;
        }

        ProgTP_Equipment *equipment = &items[index++];
        equipment->code = (uint32_t)yyjson_get_uint(code_value);
        if (!ReadRequiredString(item, "name", equipment->name, sizeof(equipment->name), error, error_size) ||
            !ReadRequiredString(item, "type", equipment->type, sizeof(equipment->type), error, error_size) ||
            !ReadRequiredString(item, "ip_address", equipment->ip_address, sizeof(equipment->ip_address), error, error_size) ||
            !ReadRequiredString(item, "mac_address", equipment->mac_address, sizeof(equipment->mac_address), error, error_size)) {
            free(items);
            yyjson_doc_free(doc);
            return false;
        }
        ReadOptionalString(item, "brand", equipment->brand, sizeof(equipment->brand));
        ReadOptionalString(item, "model", equipment->model, sizeof(equipment->model));
        ReadOptionalString(item, "location", equipment->location, sizeof(equipment->location));
        ReadOptionalString(item, "last_checked", equipment->last_checked, sizeof(equipment->last_checked));
        if (equipment->last_checked[0] == '\0') {
            ProgTP_CurrentDateString(equipment->last_checked, sizeof(equipment->last_checked));
        }
        if (!ProgTP_EquipmentStateFromString(yyjson_get_str(state_value), &equipment->state)) {
            free(items);
            yyjson_doc_free(doc);
            SetProtocolError(error, error_size, "equipment entry has unknown state");
            return false;
        }
        yyjson_val *pending_value = yyjson_obj_get(item, "has_pending_incidents");
        equipment->has_pending_incidents = yyjson_is_bool(pending_value) && yyjson_get_bool(pending_value);
    }

    bool ok = ProgTP_EquipmentInventoryReplace(inventory, items, item_count, (uint32_t)next_code_u64, error, error_size);
    free(items);
    yyjson_doc_free(doc);
    return ok;
}
