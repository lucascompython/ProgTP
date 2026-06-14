#include "progtp_text.h"

#if defined(PROGTP_HAVE_STRCASESTR) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#include <string.h>
#define PROGTP_STRCASECMP _stricmp
#define PROGTP_STRNCASECMP _strnicmp
#else
#include <strings.h>
#define PROGTP_STRCASECMP strcasecmp
#define PROGTP_STRNCASECMP strncasecmp
#endif

void ProgTP_TextCopy(char *destination, size_t destination_size, const char *source) {
    if (!destination || destination_size == 0) {
        return;
    }
    if (!source) {
        destination[0] = '\0';
        return;
    }

#if defined(PROGTP_HAVE_STRLCPY)
    strlcpy(destination, source, destination_size);
#elif defined(_MSC_VER)
    strncpy_s(destination, destination_size, source, _TRUNCATE);
#else
    size_t capacity = destination_size - 1u;
    const char *terminator = memchr(source, '\0', capacity);
    size_t length = terminator ? (size_t)(terminator - source) : capacity;
    memcpy(destination, source, length);
    destination[length] = '\0';
#endif
}

bool ProgTP_TextIsEmpty(const char *value) {
    return !value || value[0] == '\0';
}

bool ProgTP_TextEqualsIgnoreCase(const char *left, const char *right) {
    if (!left || !right) {
        return false;
    }
    return PROGTP_STRCASECMP(left, right) == 0;
}

bool ProgTP_TextContainsIgnoreCase(const char *text, const char *needle) {
    if (!text || !needle) {
        return false;
    }
    size_t needle_length = strlen(needle);
    if (needle_length == 0) {
        return true;
    }
#if defined(PROGTP_HAVE_STRCASESTR)
    return strcasestr(text, needle) != NULL;
#else
    for (const char *start = text; *start; ++start) {
        if (PROGTP_STRNCASECMP(start, needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
#endif
}

char *ProgTP_TextTrimLeft(char *value) {
    if (!value) {
        return NULL;
    }
    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    return value;
}

void ProgTP_TextTrimRight(char *value) {
    if (!value) {
        return;
    }
    size_t length = strlen(value);
    while (length > 0 && isspace((unsigned char)value[length - 1u])) {
        value[--length] = '\0';
    }
}

char *ProgTP_TextTrim(char *value) {
    char *trimmed = ProgTP_TextTrimLeft(value);
    ProgTP_TextTrimRight(trimmed);
    return trimmed;
}

void ProgTP_TextSanitizePrintable(char *value) {
    if (!value) {
        return;
    }
    for (; *value; ++value) {
        unsigned char c = (unsigned char)*value;
        if (c < 0x20u || c > 0x7Eu) {
            *value = '\0';
            return;
        }
    }
}
