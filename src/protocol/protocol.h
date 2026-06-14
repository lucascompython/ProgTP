#ifndef PROGTP_PROTOCOL_H
#define PROGTP_PROTOCOL_H

#include "config_history.h"
#include "connectivity.h"
#include "equipment_inventory.h"
#include "incident_store.h"
#include "sensor_store.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char message[128];
    char mode[32];
} ProgTP_CommandResult;

typedef enum {
    PROGTP_INCIDENT_OP_CREATE = 0,
    PROGTP_INCIDENT_OP_UPDATE,
    PROGTP_INCIDENT_OP_DELETE,
    PROGTP_INCIDENT_OP_IMPORT_LOG,
} ProgTP_IncidentOperationType;

typedef struct {
    ProgTP_IncidentOperationType operation;
    ProgTP_Incident incident;
    char log_path[128];
} ProgTP_IncidentOperationRequest;

typedef struct {
    bool success;
    uint32_t incident_number;
    uint32_t created_count;
    char message[128];
} ProgTP_IncidentOperationResponse;

typedef enum {
    PROGTP_CONFIG_OP_UNDO = 0,
    PROGTP_CONFIG_OP_REDO,
    PROGTP_CONFIG_OP_IMPORT,
    PROGTP_CONFIG_OP_DELETE,
} ProgTP_ConfigOperationType;

typedef struct {
    ProgTP_ConfigOperationType operation;
    uint32_t entry_id;
    char path[512];
} ProgTP_ConfigOperationRequest;

typedef struct {
    bool success;
    char message[128];
    ProgTP_ConfigHistory history;
} ProgTP_ConfigOperationResponse;

void ProgTP_RunLocalCommand(ProgTP_CommandResult *result);
char *ProgTP_CommandResultToJson(const ProgTP_CommandResult *result, size_t *json_length);
bool ProgTP_CommandResultFromJson(const char *json, size_t json_length, ProgTP_CommandResult *result);
void ProgTP_FormatCommandResultLabel(const ProgTP_CommandResult *result, char *buffer, size_t buffer_size);
char *ProgTP_EquipmentInventoryToJson(const ProgTP_EquipmentInventory *inventory, size_t *json_length);
bool ProgTP_EquipmentInventoryFromJson(
    const char *json,
    size_t json_length,
    ProgTP_EquipmentInventory *inventory,
    char *error,
    size_t error_size);
char *ProgTP_SensorStoreToJson(const ProgTP_SensorStore *store, size_t *json_length);
bool ProgTP_SensorStoreFromJson(
    const char *json,
    size_t json_length,
    ProgTP_SensorStore *store,
    char *error,
    size_t error_size);
char *ProgTP_SensorImportResponseToJson(
    const ProgTP_SensorImportResult *result,
    const ProgTP_SensorStore *store,
    size_t *json_length);
bool ProgTP_SensorImportResponseFromJson(
    const char *json,
    size_t json_length,
    ProgTP_SensorImportResult *result,
    ProgTP_SensorStore *store,
    char *error,
    size_t error_size);
char *ProgTP_ConnectivityRequestToJson(const ProgTP_ConnectivityRequest *request, size_t *json_length);
bool ProgTP_ConnectivityRequestFromJson(
    const char *json,
    size_t json_length,
    ProgTP_ConnectivityRequest *request,
    char *error,
    size_t error_size);
char *ProgTP_ConnectivityResultToJson(const ProgTP_ConnectivityResult *result, size_t *json_length);
bool ProgTP_ConnectivityResultFromJson(
    const char *json,
    size_t json_length,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size);
char *ProgTP_IncidentStoreToJson(const ProgTP_IncidentStore *store, size_t *json_length);
bool ProgTP_IncidentStoreFromJson(
    const char *json,
    size_t json_length,
    ProgTP_IncidentStore *store,
    char *error,
    size_t error_size);
char *ProgTP_IncidentOperationRequestToJson(const ProgTP_IncidentOperationRequest *request, size_t *json_length);
bool ProgTP_IncidentOperationRequestFromJson(
    const char *json,
    size_t json_length,
    ProgTP_IncidentOperationRequest *request,
    char *error,
    size_t error_size);
char *ProgTP_IncidentOperationResponseToJson(const ProgTP_IncidentOperationResponse *response, size_t *json_length);
bool ProgTP_IncidentOperationResponseFromJson(
    const char *json,
    size_t json_length,
    ProgTP_IncidentOperationResponse *response,
    char *error,
    size_t error_size);

char *ProgTP_ConfigHistoryToJson(const ProgTP_ConfigHistory *history, size_t *json_length);
bool ProgTP_ConfigHistoryFromJson(
    const char *json,
    size_t json_length,
    ProgTP_ConfigHistory *history,
    char *error,
    size_t error_size);
char *ProgTP_ConfigOperationRequestToJson(
    const ProgTP_ConfigOperationRequest *request,
    size_t *json_length);
bool ProgTP_ConfigOperationRequestFromJson(
    const char *json,
    size_t json_length,
    ProgTP_ConfigOperationRequest *request,
    char *error,
    size_t error_size);
char *ProgTP_ConfigOperationResponseToJson(
    const ProgTP_ConfigOperationResponse *response,
    size_t *json_length);
bool ProgTP_ConfigOperationResponseFromJson(
    const char *json,
    size_t json_length,
    ProgTP_ConfigOperationResponse *response,
    char *error,
    size_t error_size);

#endif
