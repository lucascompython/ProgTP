#ifndef PROGTP_TIME_H
#define PROGTP_TIME_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

bool ProgTP_FormatCurrentDate(char *buffer, size_t buffer_size);
bool ProgTP_FormatCurrentTimestamp(char *buffer, size_t buffer_size);
bool ProgTP_FormatFileTimestamp(time_t timestamp, char *buffer, size_t buffer_size);
bool ProgTP_FormatFileSize(size_t bytes, char *buffer, size_t buffer_size);
bool ProgTP_FormatReportMonth(char *buffer, size_t buffer_size);

#endif
