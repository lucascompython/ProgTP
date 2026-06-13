#include "incident_store.h"

#include "progtp_error.h"

#include <errno.h>
#include <stdio.h>
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
        "Ping failure",
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
