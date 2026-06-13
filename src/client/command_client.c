#include "command_client.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
} ResponseBuffer;

static size_t WriteResponse(void *contents, size_t size, size_t nmemb, void *user_data) {
    size_t byte_count = size * nmemb;
    ResponseBuffer *buffer = user_data;
    char *next = realloc(buffer->data, buffer->length + byte_count + 1);
    if (!next) {
        return 0;
    }

    buffer->data = next;
    memcpy(buffer->data + buffer->length, contents, byte_count);
    buffer->length += byte_count;
    buffer->data[buffer->length] = '\0';
    return byte_count;
}

const char *ProgTP_FindRemoteUrl(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--remote") == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static void BuildEndpoint(char *buffer, size_t buffer_size, const char *base_url, const char *path) {
    size_t length = strlen(base_url);
    const char *separator = (length > 0 && base_url[length - 1] == '/') ? "" : "/";
    snprintf(buffer, buffer_size, "%s%s%s", base_url, separator, path);
}

static bool PerformHttpRequest(
    const char *endpoint,
    const char *method,
    const char *body,
    size_t body_length,
    ResponseBuffer *response,
    char *error,
    size_t error_size) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(error, error_size, "failed to initialize curl");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, endpoint);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

    struct curl_slist *headers = NULL;
    if (body) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body_length);
    } else if (method && strcmp(method, "GET") != 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    }

    CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK || status < 200 || status >= 300) {
        snprintf(error, error_size, "request to %s failed", endpoint);
        free(response->data);
        response->data = NULL;
        response->length = 0;
        return false;
    }
    return true;
}

bool ProgTP_LoadCommandResult(int argc, char **argv, ProgTP_CommandResult *result, char *error, size_t error_size) {
    const char *remote_url = ProgTP_FindRemoteUrl(argc, argv);
    if (!remote_url) {
        ProgTP_RunLocalCommand(result);
        return true;
    }

    char endpoint[512];
    BuildEndpoint(endpoint, sizeof(endpoint), remote_url, "api/hello");

    ResponseBuffer response = {0};
    if (!PerformHttpRequest(endpoint, "GET", NULL, 0, &response, error, error_size)) {
        return false;
    }

    bool parsed = ProgTP_CommandResultFromJson(response.data, response.length, result);
    free(response.data);
    if (!parsed) {
        snprintf(error, error_size, "server returned invalid JSON");
    }
    return parsed;
}

bool ProgTP_LoadRemoteInventory(const char *remote_url, ProgTP_EquipmentInventory *inventory, char *error, size_t error_size) {
    char endpoint[512];
    BuildEndpoint(endpoint, sizeof(endpoint), remote_url, "api/inventory");

    ResponseBuffer response = {0};
    if (!PerformHttpRequest(endpoint, "GET", NULL, 0, &response, error, error_size)) {
        return false;
    }

    bool parsed = ProgTP_EquipmentInventoryFromJson(response.data, response.length, inventory, error, error_size);
    free(response.data);
    if (!parsed && error_size > 0 && error[0] == '\0') {
        snprintf(error, error_size, "server returned invalid inventory JSON");
    }
    return parsed;
}

bool ProgTP_SaveRemoteInventory(const char *remote_url, const ProgTP_EquipmentInventory *inventory, char *error, size_t error_size) {
    size_t json_length = 0;
    char *json = ProgTP_EquipmentInventoryToJson(inventory, &json_length);
    if (!json) {
        snprintf(error, error_size, "failed to serialize inventory JSON");
        return false;
    }

    char endpoint[512];
    BuildEndpoint(endpoint, sizeof(endpoint), remote_url, "api/inventory");
    ResponseBuffer response = {0};
    bool ok = PerformHttpRequest(endpoint, "PUT", json, json_length, &response, error, error_size);
    free(response.data);
    free(json);
    return ok;
}

bool ProgTP_LoadRemoteSensors(const char *remote_url, ProgTP_SensorStore *store, char *error, size_t error_size) {
    char endpoint[512];
    BuildEndpoint(endpoint, sizeof(endpoint), remote_url, "api/sensors");

    ResponseBuffer response = {0};
    if (!PerformHttpRequest(endpoint, "GET", NULL, 0, &response, error, error_size)) {
        return false;
    }

    bool parsed = ProgTP_SensorStoreFromJson(response.data, response.length, store, error, error_size);
    free(response.data);
    if (!parsed && error_size > 0 && error[0] == '\0') {
        snprintf(error, error_size, "server returned invalid sensor JSON");
    }
    return parsed;
}

bool ProgTP_RunRemoteSensorImport(
    const char *remote_url,
    ProgTP_SensorStore *store,
    ProgTP_SensorImportResult *result,
    const char *input_path,
    char *error,
    size_t error_size) {
    (void)input_path;
    char endpoint[512];
    BuildEndpoint(endpoint, sizeof(endpoint), remote_url, "api/sensors/import");
    ResponseBuffer response = {0};
    if (!PerformHttpRequest(endpoint, "POST", "{}", 2u, &response, error, error_size)) {
        return false;
    }
    bool parsed = ProgTP_SensorImportResponseFromJson(
        response.data,
        response.length,
        result,
        store,
        error,
        error_size);
    free(response.data);
    if (!parsed && error_size > 0 && error[0] == '\0') {
        snprintf(error, error_size, "server returned invalid sensor import JSON");
    }
    return parsed;
}

bool ProgTP_RunRemoteConnectivity(
    const char *remote_url,
    const ProgTP_ConnectivityRequest *request,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size) {
    size_t json_length = 0;
    char *json = ProgTP_ConnectivityRequestToJson(request, &json_length);
    if (!json) {
        snprintf(error, error_size, "failed to serialize connectivity request");
        return false;
    }
    char endpoint[512];
    BuildEndpoint(endpoint, sizeof(endpoint), remote_url, "api/connectivity/run");
    ResponseBuffer response = {0};
    bool requested = PerformHttpRequest(endpoint, "POST", json, json_length, &response, error, error_size);
    free(json);
    if (!requested) {
        return false;
    }
    bool parsed = ProgTP_ConnectivityResultFromJson(response.data, response.length, result, error, error_size);
    free(response.data);
    if (!parsed && error_size > 0 && error[0] == '\0') {
        snprintf(error, error_size, "server returned invalid connectivity JSON");
    }
    return parsed;
}

bool ProgTP_RunLocalConnectivity(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_ConnectivityRequest *request,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size) {
    return ProgTP_ConnectivityExecute(
        inventory,
        request,
        "resultado_ping.txt",
        "resultado_comando.txt",
        "log_monitorizacao.txt",
        "incidentes.dat",
        result,
        error,
        error_size);
}

bool ProgTP_RunLocalSensorImport(
    ProgTP_SensorStore *store,
    ProgTP_SensorImportResult *result,
    const char *input_path,
    char *error,
    size_t error_size) {
    return ProgTP_SensorStoreImportText(
        store,
        input_path && input_path[0] != '\0' ? input_path : "sensores_rack.txt",
        "leituras_sensores.dat",
        "log_sensores.txt",
        "incidentes.dat",
        result,
        error,
        error_size);
}
