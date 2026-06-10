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
    if (!path || !equipment) {
        ProgTP_SetError(error, error_size, "missing incident path or equipment");
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
    incident.equipment_code = equipment->code;
    snprintf(incident.source, sizeof(incident.source), "%s", equipment->name);
    snprintf(incident.type, sizeof(incident.type), "%s", "Ping failure");
    snprintf(
        incident.description,
        sizeof(incident.description),
        "Equipment %s (%s) did not respond to ping",
        equipment->name,
        equipment->ip_address);
    snprintf(incident.priority, sizeof(incident.priority), "%s", "High");
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
