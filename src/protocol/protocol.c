#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

static void CopyString(char *destination, size_t destination_size, const char *source) {
    if (destination_size == 0) {
        return;
    }
    snprintf(destination, destination_size, "%s", source ? source : "");
}

void ProgTP_RunLocalCommand(ProgTP_CommandResult *result) {
    CopyString(result->message, sizeof(result->message), "Hello from the local command runner");
    CopyString(result->mode, sizeof(result->mode), "local");
}

char *ProgTP_CommandResultToJson(const ProgTP_CommandResult *result, size_t *json_length) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        return NULL;
    }

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "message", result->message);
    yyjson_mut_obj_add_str(doc, root, "mode", result->mode);

    char *json = yyjson_mut_write(doc, 0, json_length);
    yyjson_mut_doc_free(doc);
    return json;
}

bool ProgTP_CommandResultFromJson(const char *json, size_t json_length, ProgTP_CommandResult *result) {
    yyjson_doc *doc = yyjson_read(json, json_length, 0);
    if (!doc) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *message = yyjson_obj_get(root, "message");
    yyjson_val *mode = yyjson_obj_get(root, "mode");
    if (!yyjson_is_str(message) || !yyjson_is_str(mode)) {
        yyjson_doc_free(doc);
        return false;
    }

    CopyString(result->message, sizeof(result->message), yyjson_get_str(message));
    CopyString(result->mode, sizeof(result->mode), yyjson_get_str(mode));
    yyjson_doc_free(doc);
    return true;
}

void ProgTP_FormatCommandResultLabel(const ProgTP_CommandResult *result, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%s: %s", result->mode, result->message);
}
