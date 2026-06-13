#include "sensor_store.h"

#include "incident_store.h"
#include "progtp_error.h"
#include "progtp_text.h"
#include "progtp_time.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGTP_SENSOR_FILE_MAGIC "PTPSEN1"
#define PROGTP_SENSOR_FILE_VERSION 1u

typedef struct {
    char magic[8];
    uint32_t version;
    uint64_t count;
} ProgTP_SensorFileHeader;

void ProgTP_SensorStoreInit(ProgTP_SensorStore *store) {
    memset(store, 0, sizeof(*store));
}

void ProgTP_SensorStoreDestroy(ProgTP_SensorStore *store) {
    free(store->items);
    memset(store, 0, sizeof(*store));
}

void ProgTP_SensorStoreClear(ProgTP_SensorStore *store) {
    store->length = 0;
}

static bool EnsureCapacity(ProgTP_SensorStore *store, size_t minimum, char *error, size_t error_size) {
    if (store->capacity >= minimum) {
        return true;
    }
    size_t capacity = store->capacity == 0 ? 16u : store->capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2u) {
            ProgTP_SetError(error, error_size, "sensor store is too large");
            return false;
        }
        capacity *= 2u;
    }
    ProgTP_SensorReading *items = realloc(store->items, capacity * sizeof(*items));
    if (!items) {
        ProgTP_SetError(error, error_size, "not enough memory for sensor readings");
        return false;
    }
    store->items = items;
    store->capacity = capacity;
    return true;
}

bool ProgTP_SensorStoreReplace(
    ProgTP_SensorStore *store,
    const ProgTP_SensorReading *items,
    size_t count,
    char *error,
    size_t error_size) {
    if (count > 0 && !items) {
        ProgTP_SetError(error, error_size, "missing sensor readings");
        return false;
    }
    ProgTP_SensorStore loaded;
    ProgTP_SensorStoreInit(&loaded);
    if (count > 0) {
        if (!EnsureCapacity(&loaded, count, error, error_size)) {
            ProgTP_SensorStoreDestroy(&loaded);
            return false;
        }
        memcpy(loaded.items, items, count * sizeof(*items));
        loaded.length = count;
    }
    ProgTP_SensorStoreDestroy(store);
    *store = loaded;
    return true;
}

bool ProgTP_SensorStoreCopy(ProgTP_SensorStore *destination, const ProgTP_SensorStore *source, char *error, size_t error_size) {
    return ProgTP_SensorStoreReplace(
        destination,
        source && source->length > 0 ? source->items : NULL,
        source ? source->length : 0u,
        error,
        error_size);
}

static bool AppendReading(ProgTP_SensorStore *store, const ProgTP_SensorReading *reading, char *error, size_t error_size) {
    if (!EnsureCapacity(store, store->length + 1u, error, error_size)) {
        return false;
    }
    store->items[store->length++] = *reading;
    return true;
}

bool ProgTP_SensorReadingIsAnomalous(const ProgTP_SensorReading *reading) {
    return reading &&
        (ProgTP_TextEqualsIgnoreCase(reading->state, "AVISO") ||
         ProgTP_TextEqualsIgnoreCase(reading->state, "CRITICO") ||
         ProgTP_TextEqualsIgnoreCase(reading->state, "FALHA_REDE"));
}

const ProgTP_SensorReading *ProgTP_SensorStoreFindLatestByCode(const ProgTP_SensorStore *store, const char *code) {
    if (!store || !code || code[0] == '\0') {
        return NULL;
    }
    for (size_t i = store->length; i > 0; --i) {
        const ProgTP_SensorReading *reading = &store->items[i - 1u];
        if (ProgTP_TextEqualsIgnoreCase(reading->code, code)) {
            return reading;
        }
    }
    return NULL;
}

static bool ParseReadingLine(char *line, ProgTP_SensorReading *reading, const char *timestamp, char *error, size_t error_size) {
    char *fields[5] = {0};
    char *cursor = line;
    for (size_t i = 0; i < 5u; ++i) {
        fields[i] = cursor;
        char *separator = strchr(cursor, ';');
        if (separator) {
            *separator = '\0';
            cursor = separator + 1;
        } else if (i != 4u) {
            ProgTP_SetError(error, error_size, "sensor line must have five fields");
            return false;
        }
    }
    for (size_t i = 0; i < 5u; ++i) {
        fields[i] = ProgTP_TextTrim(fields[i]);
        if (fields[i][0] == '\0') {
            ProgTP_SetError(error, error_size, "sensor line has an empty field");
            return false;
        }
    }
    char *end = NULL;
    errno = 0;
    double value = strtod(fields[2], &end);
    if (errno != 0 || !end || *ProgTP_TextTrimLeft(end) != '\0') {
        ProgTP_SetError(error, error_size, "sensor value is not numeric");
        return false;
    }
    memset(reading, 0, sizeof(*reading));
    ProgTP_TextCopy(reading->code, sizeof(reading->code), fields[0]);
    ProgTP_TextCopy(reading->type, sizeof(reading->type), fields[1]);
    reading->value = value;
    ProgTP_TextCopy(reading->unit, sizeof(reading->unit), fields[3]);
    ProgTP_TextCopy(reading->state, sizeof(reading->state), fields[4]);
    ProgTP_TextCopy(reading->imported_at, sizeof(reading->imported_at), timestamp);
    return true;
}

static bool AppendSensorLog(
    const char *path,
    const ProgTP_SensorReading *reading,
    const char *outcome,
    char *error,
    size_t error_size) {
    FILE *file = fopen(path, "a");
    if (!file) {
        ProgTP_SetError(error, error_size, "could not open sensor log");
        return false;
    }
    int written = fprintf(
        file,
        "%s;%s;%s;%.2f;%s;%s;%s\n",
        reading->imported_at,
        reading->code,
        reading->type,
        reading->value,
        reading->unit,
        reading->state,
        outcome);
    fclose(file);
    if (written <= 0) {
        ProgTP_SetError(error, error_size, "could not write sensor log");
        return false;
    }
    return true;
}

static bool CreateSensorIncident(
    const char *incident_path,
    const ProgTP_SensorReading *reading,
    char *error,
    size_t error_size) {
    char description[PROGTP_INCIDENT_DESCRIPTION_SIZE];
    snprintf(
        description,
        sizeof(description),
        "Sensor %s reported %s: %.2f %s",
        reading->code,
        reading->state,
        reading->value,
        reading->unit);
    const char *priority = ProgTP_TextEqualsIgnoreCase(reading->state, "AVISO") ? "Medium" : "High";
    return ProgTP_IncidentStoreAppendGeneric(
        incident_path,
        0u,
        reading->code,
        "Sensor anomaly",
        description,
        priority,
        reading->imported_at,
        error,
        error_size);
}

static bool ImportLines(
    ProgTP_SensorStore *store,
    const char *timestamp,
    const char *log_path,
    const char *incident_path,
    ProgTP_SensorImportResult *result,
    const char *(*next_line)(void *context),
    void *context,
    char *error,
    size_t error_size) {
    const char *line;
    uint32_t line_number = 0;
    while ((line = next_line(context)) != NULL) {
        ++line_number;
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s", line);
        ProgTP_TextTrimRight(buffer);
        char *trimmed = ProgTP_TextTrimLeft(buffer);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }
        ProgTP_SensorReading reading;
        if (!ParseReadingLine(trimmed, &reading, timestamp, error, error_size)) {
            char detail[160];
            snprintf(detail, sizeof(detail), "%s", error ? error : "invalid sensor line");
            char message[224];
            snprintf(message, sizeof(message), "line %u: %s", line_number, detail);
            ProgTP_SetError(error, error_size, message);
            return false;
        }
        if (!AppendReading(store, &reading, error, error_size)) {
            return false;
        }
        ++result->imported_count;
        bool anomalous = ProgTP_SensorReadingIsAnomalous(&reading);
        if (anomalous) {
            ++result->anomalous_count;
            if (!CreateSensorIncident(incident_path, &reading, error, error_size)) {
                return false;
            }
            ++result->incidents_created;
        }
        if (!AppendSensorLog(log_path, &reading, anomalous ? "INCIDENT_CREATED" : "OK", error, error_size)) {
            return false;
        }
    }
    return true;
}

typedef struct {
    FILE *file;
} FileLineContext;

static const char *NextFileLine(void *context) {
    FileLineContext *fc = context;
    static char line[256];
    if (fgets(line, sizeof(line), fc->file)) {
        return line;
    }
    return NULL;
}

bool ProgTP_SensorStoreImportText(
    ProgTP_SensorStore *store,
    const char *input_path,
    const char *binary_path,
    const char *log_path,
    const char *incident_path,
    ProgTP_SensorImportResult *result,
    char *error,
    size_t error_size) {
    if (!store || !input_path || !binary_path || !log_path || !incident_path || !result) {
        ProgTP_SetError(error, error_size, "missing sensor import data");
        return false;
    }
    memset(result, 0, sizeof(*result));
    FILE *file = fopen(input_path, "r");
    if (!file) {
        char message[160];
        snprintf(message, sizeof(message), "could not open %s", input_path);
        ProgTP_SetError(error, error_size, message);
        return false;
    }
    char timestamp[PROGTP_SENSOR_TIMESTAMP_SIZE];
    ProgTP_FormatCurrentTimestamp(timestamp, sizeof(timestamp));
    FileLineContext fc = { .file = file };
    bool ok = ImportLines(store, timestamp, log_path, incident_path, result, NextFileLine, &fc, error, error_size);
    fclose(file);
    if (!ok) {
        return false;
    }
    if (!ProgTP_SensorStoreSaveBinary(store, binary_path, error, error_size)) {
        return false;
    }
    snprintf(
        result->summary,
        sizeof(result->summary),
        "Imported %u sensor readings, %u anomalous, %u incidents",
        result->imported_count,
        result->anomalous_count,
        result->incidents_created);
    return true;
}

typedef struct {
    const char *cursor;
    const char *end;
    char line[256];
} ContentLineContext;

static const char *NextContentLine(void *context) {
    ContentLineContext *cc = context;
    if (cc->cursor >= cc->end) {
        return NULL;
    }
    size_t i = 0;
    while (cc->cursor < cc->end && *cc->cursor != '\n' && i < sizeof(cc->line) - 1u) {
        cc->line[i++] = *cc->cursor;
        ++cc->cursor;
    }
    cc->line[i] = '\0';
    if (cc->cursor < cc->end && *cc->cursor == '\n') {
        ++cc->cursor;
    }
    return cc->line;
}

bool ProgTP_SensorStoreImportTextFromContent(
    ProgTP_SensorStore *store,
    const char *content,
    const char *binary_path,
    const char *log_path,
    const char *incident_path,
    ProgTP_SensorImportResult *result,
    char *error,
    size_t error_size) {
    if (!store || !content || !binary_path || !log_path || !incident_path || !result) {
        ProgTP_SetError(error, error_size, "missing sensor import data");
        return false;
    }
    memset(result, 0, sizeof(*result));
    char timestamp[PROGTP_SENSOR_TIMESTAMP_SIZE];
    ProgTP_FormatCurrentTimestamp(timestamp, sizeof(timestamp));
    ContentLineContext cc = {
        .cursor = content,
        .end = content + strlen(content),
    };
    if (!ImportLines(store, timestamp, log_path, incident_path, result, NextContentLine, &cc, error, error_size)) {
        return false;
    }
    if (!ProgTP_SensorStoreSaveBinary(store, binary_path, error, error_size)) {
        return false;
    }
    snprintf(
        result->summary,
        sizeof(result->summary),
        "Imported %u sensor readings, %u anomalous, %u incidents",
        result->imported_count,
        result->anomalous_count,
        result->incidents_created);
    return true;
}

static void InitializeHeader(ProgTP_SensorFileHeader *header, uint64_t count) {
    memset(header, 0, sizeof(*header));
    memcpy(header->magic, PROGTP_SENSOR_FILE_MAGIC, sizeof(header->magic) - 1u);
    header->version = PROGTP_SENSOR_FILE_VERSION;
    header->count = count;
}

bool ProgTP_SensorStoreSaveBinary(const ProgTP_SensorStore *store, const char *path, char *error, size_t error_size) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        ProgTP_SetError(error, error_size, "could not create sensor binary file");
        return false;
    }
    ProgTP_SensorFileHeader header;
    InitializeHeader(&header, store ? (uint64_t)store->length : 0u);
    bool ok = fwrite(&header, sizeof(header), 1u, file) == 1u;
    if (ok && store && store->length > 0) {
        ok = fwrite(store->items, sizeof(store->items[0]), store->length, file) == store->length;
    }
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        ProgTP_SetError(error, error_size, "could not write sensor binary file");
    }
    return ok;
}

bool ProgTP_SensorStoreLoadBinary(ProgTP_SensorStore *store, const char *path, char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) {
            ProgTP_SensorStoreClear(store);
            return true;
        }
        ProgTP_SetError(error, error_size, "could not open sensor binary file");
        return false;
    }
    ProgTP_SensorFileHeader header;
    if (fread(&header, sizeof(header), 1u, file) != 1u ||
        memcmp(header.magic, PROGTP_SENSOR_FILE_MAGIC, sizeof(header.magic) - 1u) != 0 ||
        header.version != PROGTP_SENSOR_FILE_VERSION ||
        header.count > SIZE_MAX / sizeof(ProgTP_SensorReading)) {
        fclose(file);
        ProgTP_SetError(error, error_size, "invalid sensor binary file");
        return false;
    }
    if (!EnsureCapacity(store, (size_t)header.count, error, error_size)) {
        fclose(file);
        return false;
    }
    if (header.count > 0 &&
        fread(store->items, sizeof(store->items[0]), (size_t)header.count, file) != (size_t)header.count) {
        fclose(file);
        ProgTP_SetError(error, error_size, "could not read sensor binary file");
        return false;
    }
    store->length = (size_t)header.count;
    fclose(file);
    return true;
}

void ProgTP_SensorReadingFormatLine(const ProgTP_SensorReading *reading, char *buffer, size_t buffer_size) {
    if (!reading) {
        snprintf(buffer, buffer_size, "No sensor reading");
        return;
    }
    snprintf(
        buffer,
        buffer_size,
        "%s | %s | %.2f %s | %s | %s",
        reading->code,
        reading->type,
        reading->value,
        reading->unit,
        reading->state,
        reading->imported_at);
}
