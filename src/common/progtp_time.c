#include "progtp_time.h"

#include <stdio.h>
#include <time.h>

static bool CurrentLocalTime(struct tm *local_time) {
    if (!local_time) {
        return false;
    }
    time_t now = time(NULL);
#if defined(_WIN32)
    return localtime_s(local_time, &now) == 0;
#else
    return localtime_r(&now, local_time) != NULL;
#endif
}

bool ProgTP_FormatCurrentDate(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return false;
    }

    struct tm local_time;
    if (!CurrentLocalTime(&local_time) || strftime(buffer, buffer_size, "%Y-%m-%d", &local_time) == 0) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

bool ProgTP_FormatCurrentTimestamp(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return false;
    }

    struct tm local_time;
    if (!CurrentLocalTime(&local_time) || strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

bool ProgTP_FormatFileTimestamp(time_t timestamp, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return false;
    }
    struct tm local_time;
#if defined(_WIN32)
    if (localtime_s(&local_time, &timestamp) != 0) {
        buffer[0] = '\0';
        return false;
    }
#else
    if (localtime_r(&timestamp, &local_time) == NULL) {
        buffer[0] = '\0';
        return false;
    }
#endif
    if (strftime(buffer, buffer_size, "%d-%m-%Y %H:%M", &local_time) == 0) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

bool ProgTP_FormatFileSize(size_t bytes, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return false;
    }
    if (bytes < 1024u) {
        snprintf(buffer, buffer_size, "%zu B", bytes);
    } else if (bytes < 1024u * 1024u) {
        snprintf(buffer, buffer_size, "%.1f KB", (double)bytes / 1024.0);
    } else {
        snprintf(buffer, buffer_size, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
    }
    return true;
}
