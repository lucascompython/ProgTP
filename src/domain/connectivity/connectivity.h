#ifndef PROGTP_CONNECTIVITY_H
#define PROGTP_CONNECTIVITY_H

#include "equipment_inventory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGTP_CONNECTIVITY_COMMAND_SIZE 512u
#define PROGTP_CONNECTIVITY_SUMMARY_SIZE 256u
#define PROGTP_CONNECTIVITY_PREVIEW_SIZE 2048u
#define PROGTP_CONNECTIVITY_PATH_SIZE 128u
#define PROGTP_CONNECTIVITY_TIMESTAMP_SIZE 24u

typedef enum {
    PROGTP_CONNECTIVITY_PING_SELECTED = 0,
    PROGTP_CONNECTIVITY_PING_ALL,
    PROGTP_CONNECTIVITY_CUSTOM,
} ProgTP_ConnectivityOperation;

typedef struct {
    ProgTP_ConnectivityOperation operation;
    uint32_t equipment_code;
    char custom_command[PROGTP_CONNECTIVITY_COMMAND_SIZE];
} ProgTP_ConnectivityRequest;

typedef struct {
    bool command_succeeded;
    bool equipment_responded;
    bool inventory_changed;
    bool incident_created;
    int exit_code;
    uint32_t equipment_code;
    uint32_t executed_count;
    uint32_t responded_count;
    uint32_t failed_count;
    char command[PROGTP_CONNECTIVITY_COMMAND_SIZE];
    char summary[PROGTP_CONNECTIVITY_SUMMARY_SIZE];
    char output_preview[PROGTP_CONNECTIVITY_PREVIEW_SIZE];
    char output_path[PROGTP_CONNECTIVITY_PATH_SIZE];
    char timestamp[PROGTP_CONNECTIVITY_TIMESTAMP_SIZE];
} ProgTP_ConnectivityResult;

const char *ProgTP_ConnectivityOperationName(ProgTP_ConnectivityOperation operation);
bool ProgTP_ConnectivityOperationFromString(const char *value, ProgTP_ConnectivityOperation *operation);

bool ProgTP_ConnectivityExecute(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_ConnectivityRequest *request,
    const char *ping_output_path,
    const char *custom_output_path,
    const char *monitoring_log_path,
    const char *incident_path,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size);

#endif
