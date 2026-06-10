#ifndef PROGTP_PROTOCOL_H
#define PROGTP_PROTOCOL_H

#include "connectivity.h"
#include "equipment_inventory.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char message[128];
    char mode[32];
} ProgTP_CommandResult;

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

#endif
