#include "incident_store.h"

#include "progtp_error.h"
#include "progtp_text.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGTP_INCIDENT_FILE_MAGIC "PTPINC1"
#define PROGTP_INCIDENT_FILE_VERSION 1u

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t next_number;
    uint64_t count;
} ProgTP_IncidentFileHeader;

static bool ReadHeader(FILE *file, ProgTP_IncidentFileHeader *header) {
    return fread(header, sizeof(*header), 1u, file) == 1u &&
        memcmp(header->magic, PROGTP_INCIDENT_FILE_MAGIC, sizeof(header->magic) - 1u) == 0 &&
        header->version == PROGTP_INCIDENT_FILE_VERSION;
}

static void InitializeHeader(ProgTP_IncidentFileHeader *header) {
    memset(header, 0, sizeof(*header));
    memcpy(header->magic, PROGTP_INCIDENT_FILE_MAGIC, sizeof(header->magic) - 1u);
    header->version = PROGTP_INCIDENT_FILE_VERSION;
    header->next_number = 1u;
}

void ProgTP_IncidentStoreInit(ProgTP_IncidentStore *store) {
    memset(store, 0, sizeof(*store));
}

void ProgTP_IncidentStoreDestroy(ProgTP_IncidentStore *store) {
    ProgTP_IncidentStoreClear(store);
    memset(store, 0, sizeof(*store));
}

void ProgTP_IncidentStoreClear(ProgTP_IncidentStore *store) {
    ProgTP_IncidentNode *current = store->front;
    while (current) {
        ProgTP_IncidentNode *next = current->next;
        free(current);
        current = next;
    }
    store->front = NULL;
    store->rear = NULL;
    store->length = 0;
}

bool ProgTP_IncidentStoreCopy(ProgTP_IncidentStore *destination, const ProgTP_IncidentStore *source, char *error, size_t error_size) {
    if (!destination) {
        ProgTP_SetError(error, error_size, "missing destination incident store");
        return false;
    }
    ProgTP_IncidentStoreClear(destination);
    if (source && source->length > 0) {
        ProgTP_IncidentNode **tail_ptr = &destination->front;
        const ProgTP_IncidentNode *current = source->front;
        while (current) {
            ProgTP_IncidentNode *node = calloc(1u, sizeof(*node));
            if (!node) {
                ProgTP_IncidentStoreClear(destination);
                ProgTP_SetError(error, error_size, "not enough memory for incident copy");
                return false;
            }
            node->incident = current->incident;
            node->next = NULL;
            *tail_ptr = node;
            tail_ptr = &node->next;
            destination->rear = node;
            current = current->next;
        }
        destination->length = source->length;
    }
    return true;
}

size_t ProgTP_IncidentStoreGetCount(const ProgTP_IncidentStore *store) {
    return store ? store->length : 0u;
}

const ProgTP_Incident *ProgTP_IncidentStoreGetByIndex(const ProgTP_IncidentStore *store, size_t index) {
    if (!store || index >= store->length) {
        return NULL;
    }
    const ProgTP_IncidentNode *current = store->front;
    for (size_t i = 0; i < index && current; ++i) {
        current = current->next;
    }
    return current ? &current->incident : NULL;
}

ProgTP_Incident *ProgTP_IncidentStoreGetByIndexMut(ProgTP_IncidentStore *store, size_t index) {
    if (!store || index >= store->length) {
        return NULL;
    }
    ProgTP_IncidentNode *current = store->front;
    for (size_t i = 0; i < index && current; ++i) {
        current = current->next;
    }
    return current ? &current->incident : NULL;
}

bool ProgTP_IncidentStoreAdd(ProgTP_IncidentStore *store, const ProgTP_Incident *incident) {
    if (!store || !incident) {
        return false;
    }
    ProgTP_IncidentNode *node = calloc(1u, sizeof(*node));
    if (!node) {
        return false;
    }
    node->incident = *incident;
    node->next = NULL;
    if (!store->rear) {
        store->front = node;
        store->rear = node;
    } else {
        store->rear->next = node;
        store->rear = node;
    }
    ++store->length;
    return true;
}

bool ProgTP_IncidentStoreLoad(ProgTP_IncidentStore *store, const char *path, char *error, size_t error_size) {
    if (!store || !path) {
        ProgTP_SetError(error, error_size, "missing incident store or path");
        return false;
    }
    ProgTP_IncidentStoreClear(store);
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) {
            return true;
        }
        ProgTP_SetError(error, error_size, "could not open incident binary file");
        return false;
    }
    ProgTP_IncidentFileHeader header;
    if (fread(&header, sizeof(header), 1u, file) != 1u ||
        memcmp(header.magic, PROGTP_INCIDENT_FILE_MAGIC, sizeof(header.magic) - 1u) != 0 ||
        header.version != PROGTP_INCIDENT_FILE_VERSION ||
        header.count > SIZE_MAX / sizeof(ProgTP_Incident)) {
        fclose(file);
        ProgTP_SetError(error, error_size, "invalid incident binary file");
        return false;
    }

    ProgTP_IncidentNode **tail_ptr = &store->front;
    for (uint64_t i = 0; i < header.count; ++i) {
        ProgTP_IncidentNode *node = calloc(1u, sizeof(*node));
        if (!node || fread(&node->incident, sizeof(node->incident), 1u, file) != 1u) {
            free(node);
            fclose(file);
            ProgTP_IncidentStoreClear(store);
            ProgTP_SetError(error, error_size, "could not read incident binary file");
            return false;
        }
        node->next = NULL;
        node->incident.source[sizeof(node->incident.source) - 1u] = '\0';
        node->incident.type[sizeof(node->incident.type) - 1u] = '\0';
        node->incident.description[sizeof(node->incident.description) - 1u] = '\0';
        node->incident.priority[sizeof(node->incident.priority) - 1u] = '\0';
        node->incident.created_at[sizeof(node->incident.created_at) - 1u] = '\0';
        node->incident.completed_at[sizeof(node->incident.completed_at) - 1u] = '\0';
        node->incident.technician[sizeof(node->incident.technician) - 1u] = '\0';
        *tail_ptr = node;
        tail_ptr = &node->next;
        store->rear = node;
    }
    store->length = (size_t)header.count;
    fclose(file);
    return true;
}

bool ProgTP_IncidentStoreSave(const ProgTP_IncidentStore *store, const char *path, char *error, size_t error_size) {
    if (!store || !path) {
        ProgTP_SetError(error, error_size, "missing incident store or path");
        return false;
    }
    FILE *file = fopen(path, "wb");
    if (!file) {
        ProgTP_SetError(error, error_size, "could not create incident binary file");
        return false;
    }
    ProgTP_IncidentFileHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, PROGTP_INCIDENT_FILE_MAGIC, sizeof(header.magic) - 1u);
    header.version = PROGTP_INCIDENT_FILE_VERSION;
    header.next_number = store->rear ? store->rear->incident.number + 1u : 1u;
    header.count = (uint64_t)store->length;

    bool ok = fwrite(&header, sizeof(header), 1u, file) == 1u;
    if (ok) {
        const ProgTP_IncidentNode *current = store->front;
        while (current) {
            if (fwrite(&current->incident, sizeof(current->incident), 1u, file) != 1u) {
                ok = false;
                break;
            }
            current = current->next;
        }
    }
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        ProgTP_SetError(error, error_size, "could not write incident binary file");
    }
    return ok;
}

bool ProgTP_IncidentStoreEnqueue(
    ProgTP_IncidentStore *store,
    const ProgTP_Incident *incident,
    const char *path,
    char *error,
    size_t error_size) {
    if (!store || !incident || !path) {
        ProgTP_SetError(error, error_size, "missing incident store, incident, or path");
        return false;
    }
    ProgTP_IncidentNode *node = calloc(1u, sizeof(*node));
    if (!node) {
        ProgTP_SetError(error, error_size, "not enough memory for incident");
        return false;
    }
    node->incident = *incident;
    if (node->incident.number == 0) {
        node->incident.number = store->rear ? store->rear->incident.number + 1u : 1u;
    }
    node->next = NULL;

    if (!store->rear) {
        store->front = node;
        store->rear = node;
    } else {
        store->rear->next = node;
        store->rear = node;
    }
    ++store->length;
    return ProgTP_IncidentStoreSave(store, path, error, error_size);
}

bool ProgTP_IncidentStoreUpdate(
    ProgTP_IncidentStore *store,
    uint32_t number,
    const ProgTP_Incident *updated,
    const char *path,
    char *error,
    size_t error_size) {
    if (!store || !updated || !path) {
        ProgTP_SetError(error, error_size, "missing incident store, incident, or path");
        return false;
    }
    ProgTP_IncidentNode *prev = NULL;
    ProgTP_IncidentNode *current = store->front;
    while (current) {
        if (current->incident.number == number) {
            uint32_t original_number = current->incident.number;
            current->incident = *updated;
            current->incident.number = original_number;
            return ProgTP_IncidentStoreSave(store, path, error, error_size);
        }
        prev = current;
        current = current->next;
    }
    ProgTP_SetError(error, error_size, "incident not found");
    return false;
}

bool ProgTP_IncidentStoreDequeue(
    ProgTP_IncidentStore *store,
    uint32_t number,
    const char *path,
    char *error,
    size_t error_size) {
    if (!store || !path) {
        ProgTP_SetError(error, error_size, "missing incident store or path");
        return false;
    }
    ProgTP_IncidentNode *prev = NULL;
    ProgTP_IncidentNode *current = store->front;
    while (current) {
        if (current->incident.number == number) {
            if (prev) {
                prev->next = current->next;
            } else {
                store->front = current->next;
            }
            if (current == store->rear) {
                store->rear = prev;
            }
            free(current);
            --store->length;
            return ProgTP_IncidentStoreSave(store, path, error, error_size);
        }
        prev = current;
        current = current->next;
    }
    ProgTP_SetError(error, error_size, "incident not found");
    return false;
}

const ProgTP_Incident *ProgTP_IncidentStoreFind(const ProgTP_IncidentStore *store, uint32_t number) {
    if (!store) {
        return NULL;
    }
    const ProgTP_IncidentNode *current = store->front;
    while (current) {
        if (current->incident.number == number) {
            return &current->incident;
        }
        current = current->next;
    }
    return NULL;
}

bool ProgTP_IncidentStoreImportFromMonitoringLog(
    ProgTP_IncidentStore *store,
    const char *log_path,
    const char *incident_path,
    uint32_t *created_count,
    char *error,
    size_t error_size) {
    if (!store || !log_path || !incident_path || !created_count) {
        ProgTP_SetError(error, error_size, "missing import parameters");
        return false;
    }
    *created_count = 0;
    FILE *file = fopen(log_path, "r");
    if (!file) {
        if (errno == ENOENT) {
            return true;
        }
        char message[160];
        snprintf(message, sizeof(message), "could not open %s", log_path);
        ProgTP_SetError(error, error_size, message);
        return false;
    }
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        ProgTP_TextTrimRight(line);
        char *trimmed = ProgTP_TextTrimLeft(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }
        char *fields[8] = {0};
        char *cursor = trimmed;
        size_t field_count = 0;
        for (size_t i = 0; i < 8u && cursor; ++i) {
            fields[i] = cursor;
            ++field_count;
            char *separator = strchr(cursor, ';');
            if (separator) {
                *separator = '\0';
                cursor = separator + 1;
            } else {
                cursor = NULL;
            }
        }
        if (field_count < 7u) {
            continue;
        }
        char *end = NULL;
        errno = 0;
        long exit_code = strtol(fields[6], &end, 10);
        if (errno != 0 || !end || *ProgTP_TextTrimLeft(end) != '\0') {
            continue;
        }
        if (exit_code == 0) {
            continue;
        }
        char *end2 = NULL;
        errno = 0;
        unsigned long equipment_code = strtoul(fields[2], &end2, 10);
        if (errno != 0 || !end2 || *ProgTP_TextTrimLeft(end2) != '\0') {
            equipment_code = 0;
        }
        ProgTP_Incident incident = {0};
        incident.equipment_code = (uint32_t)equipment_code;
        snprintf(incident.source, sizeof(incident.source), "%s", trimmed);
        snprintf(incident.type, sizeof(incident.type), "%s", "Monitoring failure");
        snprintf(
            incident.description,
            sizeof(incident.description),
            "Command failed with exit code %ld: %s",
            exit_code,
            fields[1] ? fields[1] : "");
        snprintf(incident.priority, sizeof(incident.priority), "%s", "High");
        snprintf(incident.created_at, sizeof(incident.created_at), "%s", fields[0] ? fields[0] : "");
        incident.state = PROGTP_INCIDENT_PENDING;
        if (!ProgTP_IncidentStoreEnqueue(store, &incident, incident_path, error, error_size)) {
            fclose(file);
            return false;
        }
        ++(*created_count);
    }
    fclose(file);
    return true;
}

bool ProgTP_IncidentStoreAppendPingFailure(
    const char *path,
    const ProgTP_Equipment *equipment,
    const char *timestamp,
    char *error,
    size_t error_size) {
    char description[PROGTP_INCIDENT_DESCRIPTION_SIZE];
    snprintf(
        description,
        sizeof(description),
        "Equipment %s (%s) did not respond to ping",
        equipment ? equipment->name : "",
        equipment ? equipment->ip_address : "");
    return ProgTP_IncidentStoreAppendGeneric(
        path,
        equipment ? equipment->code : 0u,
        equipment ? equipment->name : "",
        "Monitoring failure",
        description,
        "High",
        timestamp,
        error,
        error_size);
}

bool ProgTP_IncidentStoreAppendGeneric(
    const char *path,
    uint32_t equipment_code,
    const char *source,
    const char *type,
    const char *description,
    const char *priority,
    const char *timestamp,
    char *error,
    size_t error_size) {
    if (!path || !source || source[0] == '\0') {
        ProgTP_SetError(error, error_size, "missing incident path or source");
        return false;
    }

    ProgTP_IncidentFileHeader header;
    InitializeHeader(&header);
    FILE *file = fopen(path, "r+b");
    if (file) {
        if (!ReadHeader(file, &header)) {
            fclose(file);
            ProgTP_SetError(error, error_size, "invalid incident binary file");
            return false;
        }
    } else if (errno == ENOENT) {
        file = fopen(path, "w+b");
        if (!file || fwrite(&header, sizeof(header), 1u, file) != 1u) {
            if (file) {
                fclose(file);
            }
            ProgTP_SetError(error, error_size, "could not create incident binary file");
            return false;
        }
    } else {
        char message[160];
        snprintf(message, sizeof(message), "could not open %s: %s", path, strerror(errno));
        ProgTP_SetError(error, error_size, message);
        return false;
    }

    ProgTP_Incident incident = {0};
    incident.number = header.next_number++;
    incident.equipment_code = equipment_code;
    snprintf(incident.source, sizeof(incident.source), "%s", source ? source : "");
    snprintf(incident.type, sizeof(incident.type), "%s", type ? type : "");
    snprintf(incident.description, sizeof(incident.description), "%s", description ? description : "");
    snprintf(incident.priority, sizeof(incident.priority), "%s", priority ? priority : "");
    snprintf(incident.created_at, sizeof(incident.created_at), "%s", timestamp ? timestamp : "");
    incident.state = PROGTP_INCIDENT_PENDING;
    ++header.count;

    bool ok = fseek(file, 0, SEEK_END) == 0 &&
        fwrite(&incident, sizeof(incident), 1u, file) == 1u &&
        fseek(file, 0, SEEK_SET) == 0 &&
        fwrite(&header, sizeof(header), 1u, file) == 1u &&
        fclose(file) == 0;
    if (!ok) {
        ProgTP_SetError(error, error_size, "failed to append incident record");
    }
    return ok;
}
