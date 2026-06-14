#ifndef PROGTP_INCIDENT_STORE_H
#define PROGTP_INCIDENT_STORE_H

#include "equipment_inventory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGTP_INCIDENT_TYPE_SIZE 32u
#define PROGTP_INCIDENT_DESCRIPTION_SIZE 160u
#define PROGTP_INCIDENT_PRIORITY_SIZE 16u
#define PROGTP_INCIDENT_TIMESTAMP_SIZE 24u
#define PROGTP_INCIDENT_TECHNICIAN_SIZE 64u

typedef enum {
    PROGTP_INCIDENT_PENDING = 0,
    PROGTP_INCIDENT_IN_PROGRESS,
    PROGTP_INCIDENT_COMPLETED,
} ProgTP_IncidentState;

typedef struct {
    uint32_t number;
    uint32_t equipment_code;
    char source[PROGTP_EQUIPMENT_NAME_SIZE];
    char type[PROGTP_INCIDENT_TYPE_SIZE];
    char description[PROGTP_INCIDENT_DESCRIPTION_SIZE];
    char priority[PROGTP_INCIDENT_PRIORITY_SIZE];
    char created_at[PROGTP_INCIDENT_TIMESTAMP_SIZE];
    char completed_at[PROGTP_INCIDENT_TIMESTAMP_SIZE];
    char technician[PROGTP_INCIDENT_TECHNICIAN_SIZE];
    ProgTP_IncidentState state;
} ProgTP_Incident;

typedef struct {
    ProgTP_Incident *items;
    size_t length;
    size_t capacity;
} ProgTP_IncidentStore;

void ProgTP_IncidentStoreInit(ProgTP_IncidentStore *store);
void ProgTP_IncidentStoreDestroy(ProgTP_IncidentStore *store);
void ProgTP_IncidentStoreClear(ProgTP_IncidentStore *store);
bool ProgTP_IncidentStoreCopy(ProgTP_IncidentStore *destination, const ProgTP_IncidentStore *source, char *error, size_t error_size);

bool ProgTP_IncidentStoreLoad(ProgTP_IncidentStore *store, const char *path, char *error, size_t error_size);
bool ProgTP_IncidentStoreSave(const ProgTP_IncidentStore *store, const char *path, char *error, size_t error_size);

bool ProgTP_IncidentStoreAppend(
    ProgTP_IncidentStore *store,
    const ProgTP_Incident *incident,
    const char *path,
    char *error,
    size_t error_size);

bool ProgTP_IncidentStoreUpdate(
    ProgTP_IncidentStore *store,
    uint32_t number,
    const ProgTP_Incident *updated,
    const char *path,
    char *error,
    size_t error_size);

bool ProgTP_IncidentStoreDelete(
    ProgTP_IncidentStore *store,
    uint32_t number,
    const char *path,
    char *error,
    size_t error_size);

const ProgTP_Incident *ProgTP_IncidentStoreFind(const ProgTP_IncidentStore *store, uint32_t number);

bool ProgTP_IncidentStoreImportFromMonitoringLog(
    ProgTP_IncidentStore *store,
    const char *log_path,
    const char *incident_path,
    uint32_t *created_count,
    char *error,
    size_t error_size);

bool ProgTP_IncidentStoreAppendPingFailure(
    const char *path,
    const ProgTP_Equipment *equipment,
    const char *timestamp,
    char *error,
    size_t error_size);
bool ProgTP_IncidentStoreAppendGeneric(
    const char *path,
    uint32_t equipment_code,
    const char *source,
    const char *type,
    const char *description,
    const char *priority,
    const char *timestamp,
    char *error,
    size_t error_size);

#endif
