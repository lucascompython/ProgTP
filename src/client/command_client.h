#ifndef PROGTP_COMMAND_CLIENT_H
#define PROGTP_COMMAND_CLIENT_H

#include "protocol.h"

#include <stdbool.h>
#include <stddef.h>

const char *ProgTP_FindRemoteUrl(int argc, char **argv);
bool ProgTP_LoadCommandResult(int argc, char **argv, ProgTP_CommandResult *result, char *error, size_t error_size);
bool ProgTP_LoadRemoteInventory(const char *remote_url, ProgTP_EquipmentInventory *inventory, char *error, size_t error_size);
bool ProgTP_SaveRemoteInventory(const char *remote_url, const ProgTP_EquipmentInventory *inventory, char *error, size_t error_size);
bool ProgTP_LoadRemoteSensors(const char *remote_url, ProgTP_SensorStore *store, char *error, size_t error_size);
bool ProgTP_RunRemoteSensorImport(
    const char *remote_url,
    ProgTP_SensorStore *store,
    ProgTP_SensorImportResult *result,
    const char *input_path,
    char *error,
    size_t error_size);
bool ProgTP_RunRemoteConnectivity(
    const char *remote_url,
    const ProgTP_ConnectivityRequest *request,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size);
bool ProgTP_RunLocalConnectivity(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_ConnectivityRequest *request,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size);
bool ProgTP_RunLocalSensorImport(
    ProgTP_SensorStore *store,
    ProgTP_SensorImportResult *result,
    const char *input_path,
    char *error,
    size_t error_size);
bool ProgTP_LoadRemoteIncidents(const char *remote_url, ProgTP_IncidentStore *store, char *error, size_t error_size);
bool ProgTP_RunRemoteIncidentOperation(
    const char *remote_url,
    const ProgTP_IncidentOperationRequest *request,
    ProgTP_IncidentOperationResponse *response,
    char *error,
    size_t error_size);
bool ProgTP_RunLocalIncidentOperation(
    ProgTP_IncidentStore *store,
    const ProgTP_IncidentOperationRequest *request,
    ProgTP_IncidentOperationResponse *response,
    char *error,
    size_t error_size);

bool ProgTP_LoadRemoteConfigHistory(
    const char *remote_url,
    ProgTP_ConfigHistory *history,
    char *error,
    size_t error_size);
bool ProgTP_RunRemoteConfigOperation(
    const char *remote_url,
    const ProgTP_ConfigOperationRequest *request,
    ProgTP_ConfigOperationResponse *response,
    char *error,
    size_t error_size);
bool ProgTP_RunLocalConfigOperation(
    ProgTP_ConfigHistory *history,
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_ConfigOperationRequest *request,
    ProgTP_ConfigOperationResponse *response,
    char *error,
    size_t error_size);

#endif
