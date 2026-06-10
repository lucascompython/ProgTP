#include "progtp_time.h"

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
