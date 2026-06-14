#include "protocol.h"

#include "progtp_error.h"
#include "progtp_text.h"
#include "progtp_time.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

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
    ProgTP_TextCopy(result->mode, sizeof(result->mode), "local");
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

    ProgTP_TextCopy(result->message, sizeof(result->message), yyjson_get_str(message));
    ProgTP_TextCopy(result->mode, sizeof(result->mode), yyjson_get_str(mode));
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

static bool ReadRequiredString(yyjson_val *object, const char *name, char *destination, size_t destination_size, char *error, size_t error_size) {
    yyjson_val *value = yyjson_obj_get(object, name);
    if (!yyjson_is_str(value)) {
        char message[96];
        snprintf(message, sizeof(message), "missing or invalid %s", name);
        ProgTP_SetError(error, error_size, message);
        return false;
    }
    ProgTP_TextCopy(destination, destination_size, yyjson_get_str(value));
    return true;
}

static void ReadOptionalString(yyjson_val *object, const char *name, char *destination, size_t destination_size) {
    yyjson_val *value = yyjson_obj_get(object, name);
    ProgTP_TextCopy(destination, destination_size, yyjson_is_str(value) ? yyjson_get_str(value) : "");
}

static bool ReadRequiredUint32(yyjson_val *object, const char *name, uint32_t *destination, char *error, size_t error_size);

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
        ProgTP_SetError(error, error_size, "invalid inventory JSON");
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *next_code_value = yyjson_obj_get(root, "next_code");
    yyjson_val *items_value = yyjson_obj_get(root, "equipment");
    if (!yyjson_is_obj(root) || !yyjson_is_uint(next_code_value) || !yyjson_is_arr(items_value)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "inventory JSON must contain next_code and equipment");
        return false;
    }

    uint64_t next_code_u64 = yyjson_get_uint(next_code_value);
    if (next_code_u64 > UINT32_MAX) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "next_code is too large");
        return false;
    }

    size_t item_count = yyjson_arr_size(items_value);
    ProgTP_Equipment *items = NULL;
    if (item_count > 0) {
        if (item_count > SIZE_MAX / sizeof(*items)) {
            yyjson_doc_free(doc);
            ProgTP_SetError(error, error_size, "inventory JSON is too large");
            return false;
        }
        items = calloc(item_count, sizeof(*items));
        if (!items) {
            yyjson_doc_free(doc);
            ProgTP_SetError(error, error_size, "not enough memory to parse inventory JSON");
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
            ProgTP_SetError(error, error_size, "equipment entry must be an object");
            return false;
        }

        yyjson_val *code_value = yyjson_obj_get(item, "code");
        yyjson_val *state_value = yyjson_obj_get(item, "state");
        if (!yyjson_is_uint(code_value) || yyjson_get_uint(code_value) > UINT32_MAX || !yyjson_is_str(state_value)) {
            free(items);
            yyjson_doc_free(doc);
            ProgTP_SetError(error, error_size, "equipment entry has invalid code or state");
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
            ProgTP_FormatCurrentDate(equipment->last_checked, sizeof(equipment->last_checked));
        }
        if (!ProgTP_EquipmentStateFromString(yyjson_get_str(state_value), &equipment->state)) {
            free(items);
            yyjson_doc_free(doc);
            ProgTP_SetError(error, error_size, "equipment entry has unknown state");
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

static yyjson_mut_val *SensorStoreToJsonValue(yyjson_mut_doc *doc, const ProgTP_SensorStore *store) {
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_val *items = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "readings", items);
    if (!store) {
        return root;
    }
    for (size_t i = 0; i < store->length; ++i) {
        const ProgTP_SensorReading *reading = &store->items[i];
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        yyjson_mut_arr_add_val(items, item);
        yyjson_mut_obj_add_str(doc, item, "code", reading->code);
        yyjson_mut_obj_add_str(doc, item, "type", reading->type);
        yyjson_mut_obj_add_real(doc, item, "value", reading->value);
        yyjson_mut_obj_add_str(doc, item, "unit", reading->unit);
        yyjson_mut_obj_add_str(doc, item, "state", reading->state);
        yyjson_mut_obj_add_str(doc, item, "imported_at", reading->imported_at);
    }
    return root;
}

char *ProgTP_SensorStoreToJson(const ProgTP_SensorStore *store, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_doc_set_root(doc, SensorStoreToJsonValue(doc, store));
    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

static bool SensorStoreFromJsonValue(
    yyjson_val *root,
    ProgTP_SensorStore *store,
    char *error,
    size_t error_size) {
    yyjson_val *items_value = yyjson_obj_get(root, "readings");
    if (!yyjson_is_obj(root) || !yyjson_is_arr(items_value)) {
        ProgTP_SetError(error, error_size, "sensor JSON must contain readings");
        return false;
    }
    size_t item_count = yyjson_arr_size(items_value);
    ProgTP_SensorReading *items = NULL;
    if (item_count > 0) {
        if (item_count > SIZE_MAX / sizeof(*items)) {
            ProgTP_SetError(error, error_size, "sensor JSON is too large");
            return false;
        }
        items = calloc(item_count, sizeof(*items));
        if (!items) {
            ProgTP_SetError(error, error_size, "not enough memory to parse sensor JSON");
            return false;
        }
    }

    size_t index = 0;
    yyjson_val *item = NULL;
    yyjson_arr_iter iter = yyjson_arr_iter_with(items_value);
    while ((item = yyjson_arr_iter_next(&iter)) != NULL) {
        yyjson_val *value = yyjson_obj_get(item, "value");
        if (!yyjson_is_obj(item) || !yyjson_is_num(value)) {
            free(items);
            ProgTP_SetError(error, error_size, "sensor entry has invalid fields");
            return false;
        }
        ProgTP_SensorReading *reading = &items[index++];
        reading->value = yyjson_get_num(value);
        if (!ReadRequiredString(item, "code", reading->code, sizeof(reading->code), error, error_size) ||
            !ReadRequiredString(item, "type", reading->type, sizeof(reading->type), error, error_size) ||
            !ReadRequiredString(item, "unit", reading->unit, sizeof(reading->unit), error, error_size) ||
            !ReadRequiredString(item, "state", reading->state, sizeof(reading->state), error, error_size) ||
            !ReadRequiredString(item, "imported_at", reading->imported_at, sizeof(reading->imported_at), error, error_size)) {
            free(items);
            return false;
        }
    }

    bool ok = ProgTP_SensorStoreReplace(store, items, item_count, error, error_size);
    free(items);
    return ok;
}

bool ProgTP_SensorStoreFromJson(
    const char *json,
    size_t json_length,
    ProgTP_SensorStore *store,
    char *error,
    size_t error_size) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        ProgTP_SetError(error, error_size, "invalid sensor JSON");
        return false;
    }
    bool ok = SensorStoreFromJsonValue(yyjson_doc_get_root(doc), store, error, error_size);
    yyjson_doc_free(doc);
    return ok;
}

char *ProgTP_SensorImportResponseToJson(
    const ProgTP_SensorImportResult *result,
    const ProgTP_SensorStore *store,
    size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_val *result_value = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "result", result_value);
    yyjson_mut_obj_add_uint(doc, result_value, "imported_count", result ? result->imported_count : 0u);
    yyjson_mut_obj_add_uint(doc, result_value, "anomalous_count", result ? result->anomalous_count : 0u);
    yyjson_mut_obj_add_uint(doc, result_value, "incidents_created", result ? result->incidents_created : 0u);
    yyjson_mut_obj_add_str(doc, result_value, "summary", result ? result->summary : "");
    yyjson_mut_obj_add_val(doc, root, "sensors", SensorStoreToJsonValue(doc, store));
    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_SensorImportResponseFromJson(
    const char *json,
    size_t json_length,
    ProgTP_SensorImportResult *result,
    ProgTP_SensorStore *store,
    char *error,
    size_t error_size) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        ProgTP_SetError(error, error_size, "invalid sensor import JSON");
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *result_value = yyjson_obj_get(root, "result");
    yyjson_val *sensors_value = yyjson_obj_get(root, "sensors");
    if (!yyjson_is_obj(root) || !yyjson_is_obj(result_value) || !yyjson_is_obj(sensors_value)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "sensor import response has invalid fields");
        return false;
    }
    memset(result, 0, sizeof(*result));
    if (!ReadRequiredUint32(result_value, "imported_count", &result->imported_count, error, error_size) ||
        !ReadRequiredUint32(result_value, "anomalous_count", &result->anomalous_count, error, error_size) ||
        !ReadRequiredUint32(result_value, "incidents_created", &result->incidents_created, error, error_size) ||
        !ReadRequiredString(result_value, "summary", result->summary, sizeof(result->summary), error, error_size) ||
        !SensorStoreFromJsonValue(sensors_value, store, error, error_size)) {
        yyjson_doc_free(doc);
        return false;
    }
    yyjson_doc_free(doc);
    return true;
}

char *ProgTP_ConnectivityRequestToJson(const ProgTP_ConnectivityRequest *request, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "operation", ProgTP_ConnectivityOperationName(request->operation));
    yyjson_mut_obj_add_uint(doc, root, "equipment_code", request->equipment_code);
    yyjson_mut_obj_add_str(doc, root, "custom_command", request->custom_command);
    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_ConnectivityRequestFromJson(
    const char *json,
    size_t json_length,
    ProgTP_ConnectivityRequest *request,
    char *error,
    size_t error_size) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        ProgTP_SetError(error, error_size, "invalid connectivity request JSON");
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *operation = yyjson_obj_get(root, "operation");
    yyjson_val *equipment_code = yyjson_obj_get(root, "equipment_code");
    yyjson_val *custom_command = yyjson_obj_get(root, "custom_command");
    if (!yyjson_is_obj(root) ||
        !yyjson_is_str(operation) ||
        !yyjson_is_uint(equipment_code) ||
        yyjson_get_uint(equipment_code) > UINT32_MAX ||
        !yyjson_is_str(custom_command) ||
        !ProgTP_ConnectivityOperationFromString(yyjson_get_str(operation), &request->operation)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "connectivity request has invalid fields");
        return false;
    }
    request->equipment_code = (uint32_t)yyjson_get_uint(equipment_code);
    ProgTP_TextCopy(request->custom_command, sizeof(request->custom_command), yyjson_get_str(custom_command));
    if (request->operation == PROGTP_CONNECTIVITY_CUSTOM && request->custom_command[0] == '\0') {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "custom command cannot be empty");
        return false;
    }
    yyjson_doc_free(doc);
    return true;
}

char *ProgTP_ConnectivityResultToJson(const ProgTP_ConnectivityResult *result, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_bool(doc, root, "command_succeeded", result->command_succeeded);
    yyjson_mut_obj_add_bool(doc, root, "equipment_responded", result->equipment_responded);
    yyjson_mut_obj_add_bool(doc, root, "inventory_changed", result->inventory_changed);
    yyjson_mut_obj_add_bool(doc, root, "incident_created", result->incident_created);
    yyjson_mut_obj_add_int(doc, root, "exit_code", result->exit_code);
    yyjson_mut_obj_add_uint(doc, root, "equipment_code", result->equipment_code);
    yyjson_mut_obj_add_uint(doc, root, "executed_count", result->executed_count);
    yyjson_mut_obj_add_uint(doc, root, "responded_count", result->responded_count);
    yyjson_mut_obj_add_uint(doc, root, "failed_count", result->failed_count);
    yyjson_mut_obj_add_str(doc, root, "command", result->command);
    yyjson_mut_obj_add_str(doc, root, "summary", result->summary);
    yyjson_mut_obj_add_str(doc, root, "output_preview", result->output_preview);
    yyjson_mut_obj_add_str(doc, root, "output_path", result->output_path);
    yyjson_mut_obj_add_str(doc, root, "timestamp", result->timestamp);
    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

static bool ReadRequiredBool(yyjson_val *object, const char *name, bool *destination, char *error, size_t error_size) {
    yyjson_val *value = yyjson_obj_get(object, name);
    if (!yyjson_is_bool(value)) {
        char message[96];
        snprintf(message, sizeof(message), "missing or invalid %s", name);
        ProgTP_SetError(error, error_size, message);
        return false;
    }
    *destination = yyjson_get_bool(value);
    return true;
}

static bool ReadRequiredUint32(yyjson_val *object, const char *name, uint32_t *destination, char *error, size_t error_size) {
    yyjson_val *value = yyjson_obj_get(object, name);
    if (!yyjson_is_uint(value) || yyjson_get_uint(value) > UINT32_MAX) {
        char message[96];
        snprintf(message, sizeof(message), "missing or invalid %s", name);
        ProgTP_SetError(error, error_size, message);
        return false;
    }
    *destination = (uint32_t)yyjson_get_uint(value);
    return true;
}

bool ProgTP_ConnectivityResultFromJson(
    const char *json,
    size_t json_length,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        ProgTP_SetError(error, error_size, "invalid connectivity result JSON");
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *exit_code = yyjson_obj_get(root, "exit_code");
    if (!yyjson_is_obj(root) || !yyjson_is_int(exit_code)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "connectivity result has invalid fields");
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->exit_code = (int)yyjson_get_sint(exit_code);
    if (!ReadRequiredBool(root, "command_succeeded", &result->command_succeeded, error, error_size) ||
        !ReadRequiredBool(root, "equipment_responded", &result->equipment_responded, error, error_size) ||
        !ReadRequiredBool(root, "inventory_changed", &result->inventory_changed, error, error_size) ||
        !ReadRequiredBool(root, "incident_created", &result->incident_created, error, error_size) ||
        !ReadRequiredUint32(root, "equipment_code", &result->equipment_code, error, error_size) ||
        !ReadRequiredUint32(root, "executed_count", &result->executed_count, error, error_size) ||
        !ReadRequiredUint32(root, "responded_count", &result->responded_count, error, error_size) ||
        !ReadRequiredUint32(root, "failed_count", &result->failed_count, error, error_size) ||
        !ReadRequiredString(root, "command", result->command, sizeof(result->command), error, error_size) ||
        !ReadRequiredString(root, "summary", result->summary, sizeof(result->summary), error, error_size) ||
        !ReadRequiredString(root, "output_preview", result->output_preview, sizeof(result->output_preview), error, error_size) ||
        !ReadRequiredString(root, "output_path", result->output_path, sizeof(result->output_path), error, error_size) ||
        !ReadRequiredString(root, "timestamp", result->timestamp, sizeof(result->timestamp), error, error_size)) {
        yyjson_doc_free(doc);
        return false;
    }
    yyjson_doc_free(doc);
    return true;
}

static const char *IncidentStateName(ProgTP_IncidentState state) {
    switch (state) {
        case PROGTP_INCIDENT_PENDING: return "pending";
        case PROGTP_INCIDENT_IN_PROGRESS: return "in_progress";
        case PROGTP_INCIDENT_COMPLETED: return "completed";
    }
    return "pending";
}

static bool IncidentStateFromString(const char *value, ProgTP_IncidentState *state) {
    if (!value) {
        return false;
    }
    if (strcmp(value, "pending") == 0) {
        *state = PROGTP_INCIDENT_PENDING;
        return true;
    }
    if (strcmp(value, "in_progress") == 0) {
        *state = PROGTP_INCIDENT_IN_PROGRESS;
        return true;
    }
    if (strcmp(value, "completed") == 0) {
        *state = PROGTP_INCIDENT_COMPLETED;
        return true;
    }
    return false;
}

char *ProgTP_IncidentStoreToJson(const ProgTP_IncidentStore *store, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_val *items = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "incidents", items);
    if (store) {
        for (size_t i = 0; i < store->length; ++i) {
            const ProgTP_Incident *incident = &store->items[i];
            yyjson_mut_val *item = yyjson_mut_obj(doc);
            yyjson_mut_arr_add_val(items, item);
            yyjson_mut_obj_add_uint(doc, item, "number", incident->number);
            yyjson_mut_obj_add_uint(doc, item, "equipment_code", incident->equipment_code);
            yyjson_mut_obj_add_str(doc, item, "source", incident->source);
            yyjson_mut_obj_add_str(doc, item, "type", incident->type);
            yyjson_mut_obj_add_str(doc, item, "description", incident->description);
            yyjson_mut_obj_add_str(doc, item, "priority", incident->priority);
            yyjson_mut_obj_add_str(doc, item, "created_at", incident->created_at);
            yyjson_mut_obj_add_str(doc, item, "completed_at", incident->completed_at);
            yyjson_mut_obj_add_str(doc, item, "technician", incident->technician);
            yyjson_mut_obj_add_str(doc, item, "state", IncidentStateName(incident->state));
        }
    }
    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_IncidentStoreFromJson(
    const char *json,
    size_t json_length,
    ProgTP_IncidentStore *store,
    char *error,
    size_t error_size) {
    if (!store) {
        ProgTP_SetError(error, error_size, "missing incident store");
        return false;
    }
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        ProgTP_SetError(error, error_size, "invalid incident store JSON");
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *items_value = yyjson_obj_get(root, "incidents");
    if (!yyjson_is_obj(root) || !yyjson_is_arr(items_value)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "incident store JSON must contain incidents array");
        return false;
    }
    ProgTP_IncidentStoreClear(store);
    yyjson_val *item = NULL;
    yyjson_arr_iter iter = yyjson_arr_iter_with(items_value);
    while ((item = yyjson_arr_iter_next(&iter)) != NULL) {
        if (!yyjson_is_obj(item)) {
            yyjson_doc_free(doc);
            ProgTP_SetError(error, error_size, "incident entry must be an object");
            return false;
        }
        ProgTP_Incident incident = {0};
        if (!ReadRequiredUint32(item, "number", &incident.number, error, error_size) ||
            !ReadRequiredUint32(item, "equipment_code", &incident.equipment_code, error, error_size) ||
            !ReadRequiredString(item, "source", incident.source, sizeof(incident.source), error, error_size) ||
            !ReadRequiredString(item, "type", incident.type, sizeof(incident.type), error, error_size) ||
            !ReadRequiredString(item, "description", incident.description, sizeof(incident.description), error, error_size) ||
            !ReadRequiredString(item, "priority", incident.priority, sizeof(incident.priority), error, error_size) ||
            !ReadRequiredString(item, "created_at", incident.created_at, sizeof(incident.created_at), error, error_size)) {
            yyjson_doc_free(doc);
            return false;
        }
        ReadOptionalString(item, "completed_at", incident.completed_at, sizeof(incident.completed_at));
        ReadOptionalString(item, "technician", incident.technician, sizeof(incident.technician));
        yyjson_val *state_value = yyjson_obj_get(item, "state");
        if (!yyjson_is_str(state_value) || !IncidentStateFromString(yyjson_get_str(state_value), &incident.state)) {
            incident.state = PROGTP_INCIDENT_PENDING;
        }
        if (store->capacity < store->length + 1u) {
            size_t capacity = store->capacity == 0 ? 16u : store->capacity * 2u;
            ProgTP_Incident *items = realloc(store->items, capacity * sizeof(*items));
            if (!items) {
                yyjson_doc_free(doc);
                ProgTP_SetError(error, error_size, "not enough memory for incidents");
                return false;
            }
            store->items = items;
            store->capacity = capacity;
        }
        store->items[store->length++] = incident;
    }
    yyjson_doc_free(doc);
    return true;
}

static const char *IncidentOperationTypeName(ProgTP_IncidentOperationType type) {
    switch (type) {
        case PROGTP_INCIDENT_OP_CREATE: return "create";
        case PROGTP_INCIDENT_OP_UPDATE: return "update";
        case PROGTP_INCIDENT_OP_DELETE: return "delete";
        case PROGTP_INCIDENT_OP_IMPORT_LOG: return "import_log";
    }
    return "create";
}

static bool IncidentOperationTypeFromString(const char *value, ProgTP_IncidentOperationType *type) {
    if (!value) {
        return false;
    }
    if (strcmp(value, "create") == 0) {
        *type = PROGTP_INCIDENT_OP_CREATE;
        return true;
    }
    if (strcmp(value, "update") == 0) {
        *type = PROGTP_INCIDENT_OP_UPDATE;
        return true;
    }
    if (strcmp(value, "delete") == 0) {
        *type = PROGTP_INCIDENT_OP_DELETE;
        return true;
    }
    if (strcmp(value, "import_log") == 0) {
        *type = PROGTP_INCIDENT_OP_IMPORT_LOG;
        return true;
    }
    return false;
}

char *ProgTP_IncidentOperationRequestToJson(const ProgTP_IncidentOperationRequest *request, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "operation", IncidentOperationTypeName(request->operation));
    yyjson_mut_obj_add_uint(doc, root, "number", request->incident.number);
    yyjson_mut_obj_add_uint(doc, root, "equipment_code", request->incident.equipment_code);
    yyjson_mut_obj_add_str(doc, root, "source", request->incident.source);
    yyjson_mut_obj_add_str(doc, root, "type", request->incident.type);
    yyjson_mut_obj_add_str(doc, root, "description", request->incident.description);
    yyjson_mut_obj_add_str(doc, root, "priority", request->incident.priority);
    yyjson_mut_obj_add_str(doc, root, "created_at", request->incident.created_at);
    yyjson_mut_obj_add_str(doc, root, "completed_at", request->incident.completed_at);
    yyjson_mut_obj_add_str(doc, root, "technician", request->incident.technician);
    yyjson_mut_obj_add_str(doc, root, "state", IncidentStateName(request->incident.state));
    yyjson_mut_obj_add_str(doc, root, "log_path", request->log_path);
    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_IncidentOperationRequestFromJson(
    const char *json,
    size_t json_length,
    ProgTP_IncidentOperationRequest *request,
    char *error,
    size_t error_size) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        ProgTP_SetError(error, error_size, "invalid incident operation request JSON");
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *op_value = yyjson_obj_get(root, "operation");
    if (!yyjson_is_obj(root) || !yyjson_is_str(op_value)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "incident operation request must contain operation");
        return false;
    }
    memset(request, 0, sizeof(*request));
    if (!IncidentOperationTypeFromString(yyjson_get_str(op_value), &request->operation)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "unknown incident operation type");
        return false;
    }
    ReadOptionalString(root, "source", request->incident.source, sizeof(request->incident.source));
    ReadOptionalString(root, "type", request->incident.type, sizeof(request->incident.type));
    ReadOptionalString(root, "description", request->incident.description, sizeof(request->incident.description));
    ReadOptionalString(root, "priority", request->incident.priority, sizeof(request->incident.priority));
    ReadOptionalString(root, "created_at", request->incident.created_at, sizeof(request->incident.created_at));
    ReadOptionalString(root, "completed_at", request->incident.completed_at, sizeof(request->incident.completed_at));
    ReadOptionalString(root, "technician", request->incident.technician, sizeof(request->incident.technician));
    ReadOptionalString(root, "log_path", request->log_path, sizeof(request->log_path));
    yyjson_val *number_value = yyjson_obj_get(root, "number");
    if (yyjson_is_uint(number_value)) {
        request->incident.number = (uint32_t)yyjson_get_uint(number_value);
    }
    yyjson_val *code_value = yyjson_obj_get(root, "equipment_code");
    if (yyjson_is_uint(code_value)) {
        request->incident.equipment_code = (uint32_t)yyjson_get_uint(code_value);
    }
    yyjson_val *state_value = yyjson_obj_get(root, "state");
    if (yyjson_is_str(state_value)) {
        IncidentStateFromString(yyjson_get_str(state_value), &request->incident.state);
    }
    yyjson_doc_free(doc);
    return true;
}

char *ProgTP_IncidentOperationResponseToJson(const ProgTP_IncidentOperationResponse *response, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_bool(doc, root, "success", response->success);
    yyjson_mut_obj_add_uint(doc, root, "incident_number", response->incident_number);
    yyjson_mut_obj_add_uint(doc, root, "created_count", response->created_count);
    yyjson_mut_obj_add_str(doc, root, "message", response->message);
    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_IncidentOperationResponseFromJson(
    const char *json,
    size_t json_length,
    ProgTP_IncidentOperationResponse *response,
    char *error,
    size_t error_size) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        ProgTP_SetError(error, error_size, "invalid incident operation response JSON");
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        ProgTP_SetError(error, error_size, "incident operation response must be an object");
        return false;
    }
    memset(response, 0, sizeof(*response));
    if (!ReadRequiredBool(root, "success", &response->success, error, error_size) ||
        !ReadRequiredUint32(root, "incident_number", &response->incident_number, error, error_size) ||
        !ReadRequiredUint32(root, "created_count", &response->created_count, error, error_size) ||
        !ReadRequiredString(root, "message", response->message, sizeof(response->message), error, error_size)) {
        yyjson_doc_free(doc);
        return false;
    }
    yyjson_doc_free(doc);
    return true;
}
