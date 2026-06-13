#ifndef PROGTP_SENSOR_STORE_H
#define PROGTP_SENSOR_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGTP_SENSOR_CODE_SIZE 32u
#define PROGTP_SENSOR_TYPE_SIZE 64u
#define PROGTP_SENSOR_UNIT_SIZE 12u
#define PROGTP_SENSOR_STATE_SIZE 24u
#define PROGTP_SENSOR_TIMESTAMP_SIZE 24u

typedef struct {
    char code[PROGTP_SENSOR_CODE_SIZE];
    char type[PROGTP_SENSOR_TYPE_SIZE];
    double value;
    char unit[PROGTP_SENSOR_UNIT_SIZE];
    char state[PROGTP_SENSOR_STATE_SIZE];
    char imported_at[PROGTP_SENSOR_TIMESTAMP_SIZE];
} ProgTP_SensorReading;

typedef struct {
    ProgTP_SensorReading *items;
    size_t length;
    size_t capacity;
} ProgTP_SensorStore;

typedef struct {
    uint32_t imported_count;
    uint32_t anomalous_count;
    uint32_t incidents_created;
    char summary[160];
} ProgTP_SensorImportResult;

void ProgTP_SensorStoreInit(ProgTP_SensorStore *store);
void ProgTP_SensorStoreDestroy(ProgTP_SensorStore *store);
void ProgTP_SensorStoreClear(ProgTP_SensorStore *store);
bool ProgTP_SensorStoreCopy(ProgTP_SensorStore *destination, const ProgTP_SensorStore *source, char *error, size_t error_size);
bool ProgTP_SensorStoreReplace(
    ProgTP_SensorStore *store,
    const ProgTP_SensorReading *items,
    size_t count,
    char *error,
    size_t error_size);
const ProgTP_SensorReading *ProgTP_SensorStoreFindLatestByCode(const ProgTP_SensorStore *store, const char *code);
bool ProgTP_SensorReadingIsAnomalous(const ProgTP_SensorReading *reading);
bool ProgTP_SensorStoreImportText(
    ProgTP_SensorStore *store,
    const char *input_path,
    const char *binary_path,
    const char *log_path,
    const char *incident_path,
    ProgTP_SensorImportResult *result,
    char *error,
    size_t error_size);
bool ProgTP_SensorStoreLoadBinary(ProgTP_SensorStore *store, const char *path, char *error, size_t error_size);
bool ProgTP_SensorStoreSaveBinary(const ProgTP_SensorStore *store, const char *path, char *error, size_t error_size);
void ProgTP_SensorReadingFormatLine(const ProgTP_SensorReading *reading, char *buffer, size_t buffer_size);

#endif
