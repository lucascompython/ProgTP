#include "protocol.h"

#include <progtp_facil_http.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGTP_SERVER_INVENTORY_PATH "equipamentos.dat"
#define PROGTP_SERVER_PING_OUTPUT_PATH "resultado_ping.txt"
#define PROGTP_SERVER_CUSTOM_OUTPUT_PATH "resultado_comando.txt"
#define PROGTP_SERVER_MONITORING_LOG_PATH "log_monitorizacao.txt"
#define PROGTP_SERVER_INCIDENT_PATH "incidentes.dat"
#define PROGTP_SERVER_CONFIG_PATH "configuracoes.dat"
#define PROGTP_SERVER_SENSOR_INPUT_PATH "sensores_rack.txt"
#define PROGTP_SERVER_SENSOR_BINARY_PATH "leituras_sensores.dat"
#define PROGTP_SERVER_SENSOR_LOG_PATH "log_sensores.txt"

static bool StrEquals(fio_str_info_s value, const char *expected) {
    size_t expected_length = strlen(expected);
    return value.len == expected_length && memcmp(value.buf, expected, expected_length) == 0;
}

static void SendText(fio_http_s *request, size_t status, const char *body) {
    fio_http_status_set(request, status);
    fio_http_response_header_set(request, FIO_STR_INFO1("content-type"), FIO_STR_INFO1("text/plain"));
    fio_http_write(request, .buf = body, .len = strlen(body), .finish = 1);
}

static void SendJson(fio_http_s *request, size_t status, char *json, size_t json_length) {
    fio_http_status_set(request, status);
    fio_http_response_header_set(request, FIO_STR_INFO1("content-type"), FIO_STR_INFO1("application/json"));
    fio_http_write(request, .buf = json, .len = json_length, .dealloc = free, .copy = 0, .finish = 1);
}

static bool LoadServerInventory(ProgTP_EquipmentInventory *inventory, char *error, size_t error_size) {
    if (ProgTP_EquipmentInventoryLoadBinary(inventory, PROGTP_SERVER_INVENTORY_PATH, error, error_size)) {
        return true;
    }
    ProgTP_EquipmentInventorySeedDefaults(inventory);
    return ProgTP_EquipmentInventorySaveBinary(inventory, PROGTP_SERVER_INVENTORY_PATH, error, error_size);
}

static bool LoadServerSensors(ProgTP_SensorStore *store, char *error, size_t error_size) {
    return ProgTP_SensorStoreLoadBinary(store, PROGTP_SERVER_SENSOR_BINARY_PATH, error, error_size);
}

static void OnApiHello(fio_http_s *request) {
    if (!StrEquals(fio_http_opath(request), "/api/hello")) {
        fio_http_send_error_response(request, 404);
        return;
    }
    if (!StrEquals(fio_http_method(request), "GET")) {
        SendText(request, 405, "method not allowed\n");
        return;
    }

    ProgTP_CommandResult result;
    ProgTP_RunLocalCommand(&result);
    snprintf(result.mode, sizeof(result.mode), "%s", "server");

    size_t json_length = 0;
    char *json = ProgTP_CommandResultToJson(&result, &json_length);
    if (!json) {
        fio_http_send_error_response(request, 500);
        return;
    }

    fio_http_response_header_set(request, FIO_STR_INFO1("content-type"), FIO_STR_INFO1("application/json"));
    fio_http_write(request, .buf = json, .len = json_length, .dealloc = free, .copy = 0, .finish = 1);
}

static void OnApiInventory(fio_http_s *request) {
    if (!StrEquals(fio_http_opath(request), "/api/inventory")) {
        fio_http_send_error_response(request, 404);
        return;
    }

    char error[256] = {0};
    ProgTP_EquipmentInventory inventory;
    ProgTP_EquipmentInventoryInit(&inventory);

    if (StrEquals(fio_http_method(request), "GET")) {
        if (!LoadServerInventory(&inventory, error, sizeof(error))) {
            ProgTP_EquipmentInventoryDestroy(&inventory);
            SendText(request, 500, error);
            return;
        }
        size_t json_length = 0;
        char *json = ProgTP_EquipmentInventoryToJson(&inventory, &json_length);
        ProgTP_EquipmentInventoryDestroy(&inventory);
        if (!json) {
            fio_http_send_error_response(request, 500);
            return;
        }
        SendJson(request, 200, json, json_length);
        return;
    }

    if (StrEquals(fio_http_method(request), "PUT") || StrEquals(fio_http_method(request), "POST")) {
        fio_http_body_seek(request, 0);
        fio_str_info_s body = fio_http_body_read(request, (size_t)-1);
        if (body.len == 0 || !ProgTP_EquipmentInventoryFromJson(body.buf, body.len, &inventory, error, sizeof(error))) {
            ProgTP_EquipmentInventoryDestroy(&inventory);
            SendText(request, 400, error[0] ? error : "invalid inventory JSON");
            return;
        }
        if (!ProgTP_EquipmentInventorySaveBinary(&inventory, PROGTP_SERVER_INVENTORY_PATH, error, sizeof(error))) {
            ProgTP_EquipmentInventoryDestroy(&inventory);
            SendText(request, 500, error);
            return;
        }
        size_t json_length = 0;
        char *json = ProgTP_EquipmentInventoryToJson(&inventory, &json_length);
        ProgTP_EquipmentInventoryDestroy(&inventory);
        if (!json) {
            fio_http_send_error_response(request, 500);
            return;
        }
        SendJson(request, 200, json, json_length);
        return;
    }

    ProgTP_EquipmentInventoryDestroy(&inventory);
    SendText(request, 405, "method not allowed\n");
}

static void OnApiConnectivityRun(fio_http_s *request) {
    if (!StrEquals(fio_http_opath(request), "/api/connectivity/run")) {
        fio_http_send_error_response(request, 404);
        return;
    }
    if (!StrEquals(fio_http_method(request), "POST")) {
        SendText(request, 405, "method not allowed\n");
        return;
    }

    fio_http_body_seek(request, 0);
    fio_str_info_s body = fio_http_body_read(request, (size_t)-1);
    ProgTP_ConnectivityRequest connectivity_request = {0};
    char error[256] = {0};
    if (body.len == 0 ||
        !ProgTP_ConnectivityRequestFromJson(
            body.buf,
            body.len,
            &connectivity_request,
            error,
            sizeof(error))) {
        SendText(request, 400, error[0] ? error : "invalid connectivity request");
        return;
    }

    ProgTP_EquipmentInventory inventory;
    ProgTP_EquipmentInventoryInit(&inventory);
    if (!LoadServerInventory(&inventory, error, sizeof(error))) {
        ProgTP_EquipmentInventoryDestroy(&inventory);
        SendText(request, 500, error);
        return;
    }

    ProgTP_ConnectivityResult result;
    if (!ProgTP_ConnectivityExecute(
            &inventory,
            &connectivity_request,
            PROGTP_SERVER_PING_OUTPUT_PATH,
            PROGTP_SERVER_CUSTOM_OUTPUT_PATH,
            PROGTP_SERVER_MONITORING_LOG_PATH,
            PROGTP_SERVER_INCIDENT_PATH,
            &result,
            error,
            sizeof(error))) {
        ProgTP_EquipmentInventoryDestroy(&inventory);
        SendText(request, 500, error);
        return;
    }
    if (result.inventory_changed &&
        !ProgTP_EquipmentInventorySaveBinary(
            &inventory,
            PROGTP_SERVER_INVENTORY_PATH,
            error,
            sizeof(error))) {
        ProgTP_EquipmentInventoryDestroy(&inventory);
        SendText(request, 500, error);
        return;
    }
    ProgTP_EquipmentInventoryDestroy(&inventory);

    size_t json_length = 0;
    char *json = ProgTP_ConnectivityResultToJson(&result, &json_length);
    if (!json) {
        fio_http_send_error_response(request, 500);
        return;
    }
    SendJson(request, 200, json, json_length);
}

static void OnApiSensors(fio_http_s *request) {
    if (!StrEquals(fio_http_opath(request), "/api/sensors")) {
        fio_http_send_error_response(request, 404);
        return;
    }
    if (!StrEquals(fio_http_method(request), "GET")) {
        SendText(request, 405, "method not allowed\n");
        return;
    }

    char error[256] = {0};
    ProgTP_SensorStore store;
    ProgTP_SensorStoreInit(&store);
    if (!LoadServerSensors(&store, error, sizeof(error))) {
        ProgTP_SensorStoreDestroy(&store);
        SendText(request, 500, error);
        return;
    }
    size_t json_length = 0;
    char *json = ProgTP_SensorStoreToJson(&store, &json_length);
    ProgTP_SensorStoreDestroy(&store);
    if (!json) {
        fio_http_send_error_response(request, 500);
        return;
    }
    SendJson(request, 200, json, json_length);
}

static void OnApiSensorsImport(fio_http_s *request) {
    if (!StrEquals(fio_http_opath(request), "/api/sensors/import")) {
        fio_http_send_error_response(request, 404);
        return;
    }
    if (!StrEquals(fio_http_method(request), "POST")) {
        SendText(request, 405, "method not allowed\n");
        return;
    }

    char error[256] = {0};
    ProgTP_SensorStore store;
    ProgTP_SensorStoreInit(&store);
    if (!LoadServerSensors(&store, error, sizeof(error))) {
        ProgTP_SensorStoreDestroy(&store);
        SendText(request, 500, error);
        return;
    }
    ProgTP_SensorImportResult result;
    if (!ProgTP_SensorStoreImportText(
            &store,
            PROGTP_SERVER_SENSOR_INPUT_PATH,
            PROGTP_SERVER_SENSOR_BINARY_PATH,
            PROGTP_SERVER_SENSOR_LOG_PATH,
            PROGTP_SERVER_INCIDENT_PATH,
            &result,
            error,
            sizeof(error))) {
        ProgTP_SensorStoreDestroy(&store);
        SendText(request, 500, error);
        return;
    }
    size_t json_length = 0;
    char *json = ProgTP_SensorImportResponseToJson(&result, &store, &json_length);
    ProgTP_SensorStoreDestroy(&store);
    if (!json) {
        fio_http_send_error_response(request, 500);
        return;
    }
    SendJson(request, 200, json, json_length);
}

static void OnApiIncidents(fio_http_s *request) {
    if (!StrEquals(fio_http_opath(request), "/api/incidents")) {
        fio_http_send_error_response(request, 404);
        return;
    }

    char error[256] = {0};
    ProgTP_IncidentStore store;
    ProgTP_IncidentStoreInit(&store);

    if (StrEquals(fio_http_method(request), "GET")) {
        if (!ProgTP_IncidentStoreLoad(&store, PROGTP_SERVER_INCIDENT_PATH, error, sizeof(error))) {
            ProgTP_IncidentStoreDestroy(&store);
            SendText(request, 500, error);
            return;
        }
        size_t json_length = 0;
        char *json = ProgTP_IncidentStoreToJson(&store, &json_length);
        ProgTP_IncidentStoreDestroy(&store);
        if (!json) {
            fio_http_send_error_response(request, 500);
            return;
        }
        SendJson(request, 200, json, json_length);
        return;
    }

    if (StrEquals(fio_http_method(request), "POST")) {
        if (!ProgTP_IncidentStoreLoad(&store, PROGTP_SERVER_INCIDENT_PATH, error, sizeof(error))) {
            ProgTP_IncidentStoreDestroy(&store);
            SendText(request, 500, error);
            return;
        }
        fio_http_body_seek(request, 0);
        fio_str_info_s body = fio_http_body_read(request, (size_t)-1);
        ProgTP_IncidentOperationRequest op_request = {0};
        if (body.len == 0 ||
            !ProgTP_IncidentOperationRequestFromJson(body.buf, body.len, &op_request, error, sizeof(error))) {
            ProgTP_IncidentStoreDestroy(&store);
            SendText(request, 400, error[0] ? error : "invalid incident operation request");
            return;
        }
        ProgTP_IncidentOperationResponse op_response = {0};
        bool ok = false;
        switch (op_request.operation) {
            case PROGTP_INCIDENT_OP_CREATE: {
                ProgTP_Incident incident = op_request.incident;
                incident.number = 0;
                ok = ProgTP_IncidentStoreAppend(&store, &incident, PROGTP_SERVER_INCIDENT_PATH, error, sizeof(error));
                if (ok) {
                    op_response.success = true;
                    op_response.incident_number = store.items[store.length - 1u].number;
                    snprintf(op_response.message, sizeof(op_response.message), "Incident #%u created", op_response.incident_number);
                }
                break;
            }
            case PROGTP_INCIDENT_OP_UPDATE:
                ok = ProgTP_IncidentStoreUpdate(&store, op_request.incident.number, &op_request.incident, PROGTP_SERVER_INCIDENT_PATH, error, sizeof(error));
                if (ok) {
                    op_response.success = true;
                    op_response.incident_number = op_request.incident.number;
                    snprintf(op_response.message, sizeof(op_response.message), "Incident #%u updated", op_response.incident_number);
                }
                break;
            case PROGTP_INCIDENT_OP_DELETE:
                ok = ProgTP_IncidentStoreDelete(&store, op_request.incident.number, PROGTP_SERVER_INCIDENT_PATH, error, sizeof(error));
                if (ok) {
                    op_response.success = true;
                    op_response.incident_number = op_request.incident.number;
                    snprintf(op_response.message, sizeof(op_response.message), "Incident #%u deleted", op_response.incident_number);
                }
                break;
            case PROGTP_INCIDENT_OP_IMPORT_LOG: {
                const char *log_path = op_request.log_path[0] != '\0' ? op_request.log_path : PROGTP_SERVER_MONITORING_LOG_PATH;
                uint32_t created_count = 0;
                ok = ProgTP_IncidentStoreImportFromMonitoringLog(&store, log_path, PROGTP_SERVER_INCIDENT_PATH, &created_count, error, sizeof(error));
                if (ok) {
                    op_response.success = true;
                    op_response.created_count = created_count;
                    snprintf(op_response.message, sizeof(op_response.message), "Imported %u incidents from log", created_count);
                }
                break;
            }
        }
        ProgTP_IncidentStoreDestroy(&store);
        if (!ok) {
            SendText(request, 500, error);
            return;
        }
        size_t json_length = 0;
        char *json = ProgTP_IncidentOperationResponseToJson(&op_response, &json_length);
        if (!json) {
            fio_http_send_error_response(request, 500);
            return;
        }
        SendJson(request, 200, json, json_length);
        return;
    }

    ProgTP_IncidentStoreDestroy(&store);
    SendText(request, 405, "method not allowed\n");
}

static void OnApiConfig(fio_http_s *request) {
    if (!StrEquals(fio_http_opath(request), "/api/config")) {
        fio_http_send_error_response(request, 404);
        return;
    }

    char error[256] = {0};
    ProgTP_EquipmentInventory inventory;
    ProgTP_EquipmentInventoryInit(&inventory);
    if (!ProgTP_EquipmentInventoryLoadBinary(&inventory, PROGTP_SERVER_INVENTORY_PATH, error, sizeof(error))) {
        ProgTP_EquipmentInventoryDestroy(&inventory);
        SendText(request, 500, error[0] ? error : "could not load equipment inventory");
        return;
    }
    ProgTP_ConfigHistory history;
    ProgTP_ConfigHistoryInit(&history);
    if (!ProgTP_ConfigHistoryLoad(&history, PROGTP_SERVER_CONFIG_PATH, error, sizeof(error))) {
        ProgTP_ConfigHistoryDestroy(&history);
        ProgTP_EquipmentInventoryDestroy(&inventory);
        SendText(request, 500, error[0] ? error : "could not load config history");
        return;
    }

    if (StrEquals(fio_http_method(request), "GET")) {
        size_t json_length = 0;
        char *json = ProgTP_ConfigHistoryToJson(&history, &json_length);
        ProgTP_ConfigHistoryDestroy(&history);
        ProgTP_EquipmentInventoryDestroy(&inventory);
        if (!json) {
            fio_http_send_error_response(request, 500);
            return;
        }
        SendJson(request, 200, json, json_length);
        return;
    }

    if (StrEquals(fio_http_method(request), "POST")) {
        fio_http_body_seek(request, 0);
        fio_str_info_s body = fio_http_body_read(request, (size_t)-1);
        ProgTP_ConfigOperationRequest op_request = {0};
        if (body.len == 0 ||
            !ProgTP_ConfigOperationRequestFromJson(body.buf, body.len, &op_request, error, sizeof(error))) {
            ProgTP_ConfigHistoryDestroy(&history);
            ProgTP_EquipmentInventoryDestroy(&inventory);
            SendText(request, 400, error[0] ? error : "invalid config operation request");
            return;
        }
        ProgTP_ConfigOperationResponse op_response = {0};
        bool ok = false;
        if (op_request.operation == PROGTP_CONFIG_OP_UNDO) {
            ok = ProgTP_ConfigHistoryUndo(&history, &inventory, error, sizeof(error));
            if (ok) {
                snprintf(op_response.message, sizeof(op_response.message), "Undid last change");
            }
        } else if (op_request.operation == PROGTP_CONFIG_OP_REDO) {
            ok = ProgTP_ConfigHistoryRedo(&history, &inventory, error, sizeof(error));
            if (ok) {
                snprintf(op_response.message, sizeof(op_response.message), "Redid change");
            }
        } else if (op_request.operation == PROGTP_CONFIG_OP_IMPORT) {
            const char *import_path = op_request.path[0] != '\0'
                ? op_request.path
                : PROGTP_SERVER_CONFIG_PATH;
            ok = ProgTP_ConfigHistoryImportFromFile(&history, import_path, error, sizeof(error));
            if (ok) {
                snprintf(op_response.message, sizeof(op_response.message), "Imported config history from %.280s", import_path);
            }
        } else if (op_request.operation == PROGTP_CONFIG_OP_DELETE) {
            ok = ProgTP_ConfigHistoryDeleteById(&history, op_request.entry_id, error, sizeof(error));
            if (ok) {
                snprintf(op_response.message, sizeof(op_response.message), "Removed config entry #%u", op_request.entry_id);
            }
        } else {
            snprintf(error, sizeof(error), "unknown config operation");
        }
        if (ok) {
            op_response.success = true;
            if (!ProgTP_ConfigHistorySave(&history, PROGTP_SERVER_CONFIG_PATH, error, sizeof(error))) {
                ok = false;
            } else if (!ProgTP_EquipmentInventorySaveBinary(&inventory, PROGTP_SERVER_INVENTORY_PATH, error, sizeof(error))) {
                ok = false;
            }
        }
        if (ok) {
            op_response.history = history;
            size_t json_length = 0;
            char *json = ProgTP_ConfigOperationResponseToJson(&op_response, &json_length);
            ProgTP_ConfigHistoryDestroy(&history);
            ProgTP_EquipmentInventoryDestroy(&inventory);
            if (!json) {
                fio_http_send_error_response(request, 500);
                return;
            }
            SendJson(request, 200, json, json_length);
            return;
        }
        ProgTP_ConfigHistoryDestroy(&history);
        ProgTP_EquipmentInventoryDestroy(&inventory);
        SendText(request, 500, error[0] ? error : "config operation failed");
        return;
    }

    ProgTP_ConfigHistoryDestroy(&history);
    ProgTP_EquipmentInventoryDestroy(&inventory);
    SendText(request, 405, "method not allowed\n");
}

static void OnStaticMiss(fio_http_s *request) {
    SendText(request, 404, "ProgTP server. Try /api/inventory, /api/sensors, /api/connectivity/run, /api/hello, or /index.html\n");
}

int main(int argc, char **argv) {
    const char *port = "3000";
    const char *public_folder = ".";
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0) {
            port = argv[i + 1];
        } else if (strcmp(argv[i], "--public") == 0) {
            public_folder = argv[i + 1];
        }
    }

    char listen_url[96];
    snprintf(listen_url, sizeof(listen_url), "0.0.0.0:%s", port);

    fio_http_mimetype_register((char *)"wasm", 4, FIO_STR_INFO1("application/wasm"));

    fio_http_listener_s *listener = fio_http_listen(
        listen_url,
        .on_http = OnStaticMiss,
        .public_folder = FIO_STR_INFO1((char *)public_folder),
        .log = 1);
    if (!listener) {
        fprintf(stderr, "failed to listen on port %s\n", port);
        return 1;
    }

    if (fio_http_route(
            listener,
            "/api/hello",
            .on_http = OnApiHello,
            .public_folder = FIO_STR_INFO2("", 0)) != 0) {
        fprintf(stderr, "failed to register /api/hello route\n");
        return 1;
    }
    if (fio_http_route(
            listener,
            "/api/inventory",
            .on_http = OnApiInventory,
            .public_folder = FIO_STR_INFO2("", 0)) != 0) {
        fprintf(stderr, "failed to register /api/inventory route\n");
        return 1;
    }
    if (fio_http_route(
            listener,
            "/api/connectivity/run",
            .on_http = OnApiConnectivityRun,
            .public_folder = FIO_STR_INFO2("", 0)) != 0) {
        fprintf(stderr, "failed to register /api/connectivity/run route\n");
        return 1;
    }
    if (fio_http_route(
            listener,
            "/api/sensors",
            .on_http = OnApiSensors,
            .public_folder = FIO_STR_INFO2("", 0)) != 0) {
        fprintf(stderr, "failed to register /api/sensors route\n");
        return 1;
    }
    if (fio_http_route(
            listener,
            "/api/sensors/import",
            .on_http = OnApiSensorsImport,
            .public_folder = FIO_STR_INFO2("", 0)) != 0) {
        fprintf(stderr, "failed to register /api/sensors/import route\n");
        return 1;
    }
    if (fio_http_route(
            listener,
            "/api/incidents",
            .on_http = OnApiIncidents,
            .public_folder = FIO_STR_INFO2("", 0)) != 0) {
        fprintf(stderr, "failed to register /api/incidents route\n");
        return 1;
    }
    if (fio_http_route(
            listener,
            "/api/config",
            .on_http = OnApiConfig,
            .public_folder = FIO_STR_INFO2("", 0)) != 0) {
        fprintf(stderr, "failed to register /api/config route\n");
        return 1;
    }

    printf("ProgTP server listening on http://localhost:%s\n", port);
    fio_io_start(0);
    return 0;
}
