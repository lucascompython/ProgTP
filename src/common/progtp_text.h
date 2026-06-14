#ifndef PROGTP_TEXT_H
#define PROGTP_TEXT_H

#include <stdbool.h>
#include <stddef.h>

void ProgTP_TextCopy(char *destination, size_t destination_size, const char *source);
bool ProgTP_TextIsEmpty(const char *value);
bool ProgTP_TextEqualsIgnoreCase(const char *left, const char *right);
bool ProgTP_TextContainsIgnoreCase(const char *text, const char *needle);
char *ProgTP_TextTrimLeft(char *value);
void ProgTP_TextTrimRight(char *value);
char *ProgTP_TextTrim(char *value);
void ProgTP_TextSanitizePrintable(char *value);

#endif
