#include "connectivity.h"

#include <string.h>

const char *ProgTP_ConnectivityOperationName(ProgTP_ConnectivityOperation operation) {
    switch (operation) {
        case PROGTP_CONNECTIVITY_PING_SELECTED: return "ping_selected";
        case PROGTP_CONNECTIVITY_PING_ALL: return "ping_all";
        case PROGTP_CONNECTIVITY_CUSTOM: return "custom";
    }
    return "unknown";
}

bool ProgTP_ConnectivityOperationFromString(const char *value, ProgTP_ConnectivityOperation *operation) {
    if (!value || !operation) {
        return false;
    }
    if (strcmp(value, "ping_selected") == 0) {
        *operation = PROGTP_CONNECTIVITY_PING_SELECTED;
    } else if (strcmp(value, "ping_all") == 0) {
        *operation = PROGTP_CONNECTIVITY_PING_ALL;
    } else if (strcmp(value, "custom") == 0) {
        *operation = PROGTP_CONNECTIVITY_CUSTOM;
    } else {
        return false;
    }
    return true;
}
