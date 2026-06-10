#ifndef PROGTP_TIME_H
#define PROGTP_TIME_H

#include <stdbool.h>
#include <stddef.h>

bool ProgTP_FormatCurrentDate(char *buffer, size_t buffer_size);
bool ProgTP_FormatCurrentTimestamp(char *buffer, size_t buffer_size);

#endif
