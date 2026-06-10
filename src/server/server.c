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

static void OnStaticMiss(fio_http_s *request) {
    SendText(request, 404, "ProgTP server. Try /api/inventory, /api/connectivity/run, /api/hello, or /index.html\n");
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

    printf("ProgTP server listening on http://localhost:%s\n", port);
    fio_io_start(0);
    return 0;
}
