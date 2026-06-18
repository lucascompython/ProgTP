#include "report_generator.h"

#include "progtp_time.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

bool ProgTP_GenerateNetworkStatusReport(
    const ProgTP_EquipmentInventory *inventory,
    const ProgTP_IncidentStore *incidents,
    const ProgTP_SensorStore *sensors,
    const char *output_path,
    char *error,
    size_t error_size) {
    if (!inventory || !incidents || !sensors || !output_path) {
        snprintf(error, error_size, "missing parameters for network status report");
        return false;
    }

    FILE *file = fopen(output_path, "w");
    if (!file) {
        snprintf(error, error_size, "could not create %s: %s", output_path, strerror(errno));
        return false;
    }

    char timestamp[32];
    ProgTP_FormatCurrentDate(timestamp, sizeof(timestamp));
    fprintf(file, "Network Status Report\n");
    fprintf(file, "Date: %s\n", timestamp);
    fprintf(file, "=========================================\n\n");

    size_t count = ProgTP_EquipmentInventoryGetCount(inventory);
    size_t operational = 0;
    size_t failed = 0;
    size_t maintenance = 0;
    size_t disabled = 0;

    for (size_t i = 0; i < count; ++i) {
        const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(inventory, i);
        if (!equipment) {
            continue;
        }
        switch (equipment->state) {
            case PROGTP_EQUIPMENT_OPERATIONAL: ++operational; break;
            case PROGTP_EQUIPMENT_FAILED: ++failed; break;
            case PROGTP_EQUIPMENT_MAINTENANCE: ++maintenance; break;
            case PROGTP_EQUIPMENT_DISABLED: ++disabled; break;
        }
    }

    fprintf(file, "Equipment Summary\n");
    fprintf(file, "-----------------\n");
    fprintf(file, "Total: %zu\n", count);
    fprintf(file, "Operational: %zu\n", operational);
    fprintf(file, "Failed: %zu\n", failed);
    fprintf(file, "Maintenance: %zu\n", maintenance);
    fprintf(file, "Disabled: %zu\n\n", disabled);

    fprintf(file, "Operational Equipment\n");
    fprintf(file, "---------------------\n");
    if (operational == 0) {
        fprintf(file, "(none)\n");
    } else {
        for (size_t i = 0; i < count; ++i) {
            const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(inventory, i);
            if (equipment && equipment->state == PROGTP_EQUIPMENT_OPERATIONAL) {
                fprintf(file, "#%u | %s | %s | %s %s | IP %s | last check %s\n",
                    equipment->code, equipment->name, equipment->type,
                    equipment->brand, equipment->model,
                    equipment->ip_address, equipment->last_checked);
            }
        }
    }

    fprintf(file, "\nFailed Equipment\n");
    fprintf(file, "----------------\n");
    if (failed == 0) {
        fprintf(file, "(none)\n");
    } else {
        for (size_t i = 0; i < count; ++i) {
            const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(inventory, i);
            if (equipment && equipment->state == PROGTP_EQUIPMENT_FAILED) {
                fprintf(file, "#%u | %s | %s | %s %s | IP %s | last check %s\n",
                    equipment->code, equipment->name, equipment->type,
                    equipment->brand, equipment->model,
                    equipment->ip_address, equipment->last_checked);
            }
        }
    }

    fprintf(file, "\nMaintenance Equipment\n");
    fprintf(file, "---------------------\n");
    if (maintenance == 0) {
        fprintf(file, "(none)\n");
    } else {
        for (size_t i = 0; i < count; ++i) {
            const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(inventory, i);
            if (equipment && equipment->state == PROGTP_EQUIPMENT_MAINTENANCE) {
                fprintf(file, "#%u | %s | %s | %s %s | IP %s | last check %s\n",
                    equipment->code, equipment->name, equipment->type,
                    equipment->brand, equipment->model,
                    equipment->ip_address, equipment->last_checked);
            }
        }
    }

    fprintf(file, "\nDisabled Equipment\n");
    fprintf(file, "------------------\n");
    if (disabled == 0) {
        fprintf(file, "(none)\n");
    } else {
        for (size_t i = 0; i < count; ++i) {
            const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(inventory, i);
            if (equipment && equipment->state == PROGTP_EQUIPMENT_DISABLED) {
                fprintf(file, "#%u | %s | %s | %s %s | IP %s | last check %s\n",
                    equipment->code, equipment->name, equipment->type,
                    equipment->brand, equipment->model,
                    equipment->ip_address, equipment->last_checked);
            }
        }
    }

    fprintf(file, "\nPending Incidents\n");
    fprintf(file, "-----------------\n");
    {
        size_t incident_count = ProgTP_IncidentStoreGetCount(incidents);
        size_t pending = 0;
        for (size_t i = 0; i < incident_count; ++i) {
            const ProgTP_Incident *incident = ProgTP_IncidentStoreGetByIndex(incidents, i);
            if (incident && incident->state == PROGTP_INCIDENT_PENDING) {
                ++pending;
            }
        }
        if (pending == 0) {
            fprintf(file, "(none)\n");
        } else {
            fprintf(file, "Total pending: %zu\n\n", pending);
            for (size_t i = 0; i < incident_count; ++i) {
                const ProgTP_Incident *incident = ProgTP_IncidentStoreGetByIndex(incidents, i);
                if (incident && incident->state == PROGTP_INCIDENT_PENDING) {
                    fprintf(file, "#%u | Eq#%u | %s | Type: %s | Priority: %s | Created: %s | Tech: %s\n",
                        incident->number, incident->equipment_code, incident->source,
                        incident->type, incident->priority,
                        incident->created_at, incident->technician);
                }
            }
        }
    }

    fprintf(file, "\nAnomalous Sensor Readings\n");
    fprintf(file, "-------------------------\n");
    {
        size_t anomalous = 0;
        for (size_t i = 0; i < sensors->length; ++i) {
            if (ProgTP_SensorReadingIsAnomalous(&sensors->items[i])) {
                ++anomalous;
            }
        }
        if (anomalous == 0) {
            fprintf(file, "(none)\n");
        } else {
            fprintf(file, "Total anomalous: %zu\n\n", anomalous);
            for (size_t i = 0; i < sensors->length; ++i) {
                if (ProgTP_SensorReadingIsAnomalous(&sensors->items[i])) {
                    fprintf(file, "%s | %s | %.2f %s | %s | %s\n",
                        sensors->items[i].code, sensors->items[i].type,
                        sensors->items[i].value, sensors->items[i].unit,
                        sensors->items[i].state, sensors->items[i].imported_at);
                }
            }
        }
    }

    fprintf(file, "\n=========================================\n");
    fprintf(file, "End of report\n");

    if (fclose(file) != 0) {
        snprintf(error, error_size, "could not close %s", output_path);
        return false;
    }
    return true;
}

bool ProgTP_GenerateIncidentReport(
    const ProgTP_IncidentStore *incidents,
    const char *output_path,
    char *error,
    size_t error_size) {
    if (!incidents || !output_path) {
        snprintf(error, error_size, "missing parameters for incident report");
        return false;
    }

    FILE *file = fopen(output_path, "w");
    if (!file) {
        snprintf(error, error_size, "could not create %s: %s", output_path, strerror(errno));
        return false;
    }

    char timestamp[32];
    ProgTP_FormatCurrentDate(timestamp, sizeof(timestamp));
    fprintf(file, "Monthly Incident Report\n");
    fprintf(file, "Date: %s\n", timestamp);
    fprintf(file, "=========================================\n\n");

    size_t incident_count = ProgTP_IncidentStoreGetCount(incidents);
    fprintf(file, "Total incidents: %zu\n\n", incident_count);

    size_t pending = 0;
    size_t in_progress = 0;
    size_t completed = 0;

    for (size_t i = 0; i < incident_count; ++i) {
        const ProgTP_Incident *incident = ProgTP_IncidentStoreGetByIndex(incidents, i);
        if (!incident) {
            continue;
        }
        switch (incident->state) {
            case PROGTP_INCIDENT_PENDING: ++pending; break;
            case PROGTP_INCIDENT_IN_PROGRESS: ++in_progress; break;
            case PROGTP_INCIDENT_COMPLETED: ++completed; break;
        }
    }

    fprintf(file, "By State\n");
    fprintf(file, "--------\n");
    fprintf(file, "Pending: %zu\n", pending);
    fprintf(file, "In Progress: %zu\n", in_progress);
    fprintf(file, "Completed: %zu\n\n", completed);

    fprintf(file, "By Priority\n");
    fprintf(file, "-----------\n");
    {
        size_t low = 0;
        size_t medium = 0;
        size_t high = 0;
        for (size_t i = 0; i < incident_count; ++i) {
            const ProgTP_Incident *incident = ProgTP_IncidentStoreGetByIndex(incidents, i);
            if (!incident) {
                continue;
            }
            if (strcmp(incident->priority, "Low") == 0) {
                ++low;
            } else if (strcmp(incident->priority, "Medium") == 0) {
                ++medium;
            } else if (strcmp(incident->priority, "High") == 0) {
                ++high;
            }
        }
        fprintf(file, "Low: %zu\n", low);
        fprintf(file, "Medium: %zu\n", medium);
        fprintf(file, "High: %zu\n\n", high);
    }

    fprintf(file, "Detailed Incidents\n");
    fprintf(file, "------------------\n");
    if (incident_count == 0) {
        fprintf(file, "(none)\n");
    } else {
        for (size_t i = 0; i < incident_count; ++i) {
            const ProgTP_Incident *incident = ProgTP_IncidentStoreGetByIndex(incidents, i);
            if (!incident) {
                continue;
            }
            const char *state_name = "Pending";
            if (incident->state == PROGTP_INCIDENT_IN_PROGRESS) {
                state_name = "In Progress";
            } else if (incident->state == PROGTP_INCIDENT_COMPLETED) {
                state_name = "Completed";
            }
            fprintf(file, "#%u | Eq#%u | %s | Type: %s | Priority: %s | State: %s | Created: %s | Tech: %s\n\n",
                incident->number, incident->equipment_code, incident->source,
                incident->type, incident->priority, state_name,
                incident->created_at, incident->technician);
            fprintf(file, "  Description: %s\n\n", incident->description);
        }
    }

    fprintf(file, "=========================================\n");
    fprintf(file, "End of report\n");

    if (fclose(file) != 0) {
        snprintf(error, error_size, "could not close %s", output_path);
        return false;
    }
    return true;
}
