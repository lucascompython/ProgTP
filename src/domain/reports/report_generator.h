#ifndef PROGTP_REPORT_GENERATOR_H
#define PROGTP_REPORT_GENERATOR_H

#include "equipment_inventory.h"
#include "incident_store.h"
#include "sensor_store.h"

#include <stdbool.h>
#include <stddef.h>

bool ProgTP_GenerateNetworkStatusReport(
    const ProgTP_EquipmentInventory *inventory,
    const ProgTP_IncidentStore *incidents,
    const ProgTP_SensorStore *sensors,
    const char *output_path,
    char *error,
    size_t error_size);

bool ProgTP_GenerateIncidentReport(
    const ProgTP_IncidentStore *incidents,
    const char *output_path,
    char *error,
    size_t error_size);

#endif
