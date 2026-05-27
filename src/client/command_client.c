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

static const char *FindRemoteUrl(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--remote") == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static void BuildEndpoint(char *buffer, size_t buffer_size, const char *base_url) {
    size_t length = strlen(base_url);
    const char *separator = (length > 0 && base_url[length - 1] == '/') ? "" : "/";
    snprintf(buffer, buffer_size, "%s%sapi/hello", base_url, separator);
}

bool ProgTP_LoadCommandResult(int argc, char **argv, ProgTP_CommandResult *result, char *error, size_t error_size) {
    const char *remote_url = FindRemoteUrl(argc, argv);
    if (!remote_url) {
        ProgTP_RunLocalCommand(result);
        return true;
    }

    char endpoint[512];
    BuildEndpoint(endpoint, sizeof(endpoint), remote_url);

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(error, error_size, "failed to initialize curl");
        return false;
    }

    ResponseBuffer response = {0};
    curl_easy_setopt(curl, CURLOPT_URL, endpoint);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK || status < 200 || status >= 300) {
        snprintf(error, error_size, "request to %s failed", endpoint);
        free(response.data);
        return false;
    }

    bool parsed = ProgTP_CommandResultFromJson(response.data, response.length, result);
    free(response.data);
    if (!parsed) {
        snprintf(error, error_size, "server returned invalid JSON");
    }
    return parsed;
}
