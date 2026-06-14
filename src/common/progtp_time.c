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

bool ProgTP_FormatReportMonth(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 16) {
        return false;
    }
    struct tm local_time;
    if (!CurrentLocalTime(&local_time)) {
        buffer[0] = '\0';
        return false;
    }
    int month = local_time.tm_mon + 1;
    int year = local_time.tm_year + 1900;
    const char *month_name;
    switch (month) {
        case 1: month_name = "janeiro"; break;
        case 2: month_name = "fevereiro"; break;
        case 3: month_name = "marco"; break;
        case 4: month_name = "abril"; break;
        case 5: month_name = "maio"; break;
        case 6: month_name = "junho"; break;
        case 7: month_name = "julho"; break;
        case 8: month_name = "agosto"; break;
        case 9: month_name = "setembro"; break;
        case 10: month_name = "outubro"; break;
        case 11: month_name = "novembro"; break;
        case 12: month_name = "dezembro"; break;
        default: month_name = "mes"; break;
    }
    int written = snprintf(buffer, buffer_size, "%s_%d", month_name, year);
    return written > 0 && (size_t)written < buffer_size;
}
