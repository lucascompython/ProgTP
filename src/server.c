#include "protocol.h"

#include <progtp_facil_http.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool StrEquals(fio_str_info_s value, const char *expected) {
    size_t expected_length = strlen(expected);
    return value.len == expected_length && memcmp(value.buf, expected, expected_length) == 0;
}

static void SendText(fio_http_s *request, size_t status, const char *body) {
    fio_http_status_set(request, status);
    fio_http_response_header_set(request, FIO_STR_INFO1("content-type"), FIO_STR_INFO1("text/plain"));
    fio_http_write(request, .buf = body, .len = strlen(body), .finish = 1);
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

static void OnStaticMiss(fio_http_s *request) {
    SendText(request, 404, "ProgTP server. Try /api/hello or /index.html\n");
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

    printf("ProgTP server listening on http://localhost:%s\n", port);
    fio_io_start(0);
    return 0;
}
