#include "progtp_error.h"

#include "progtp_text.h"

void ProgTP_SetError(char *error, size_t error_size, const char *message) {
    ProgTP_TextCopy(error, error_size, message ? message : "unknown error");
}
