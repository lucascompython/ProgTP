#ifndef PROGTP_COMMAND_CLIENT_H
#define PROGTP_COMMAND_CLIENT_H

#include "protocol.h"

#include <stdbool.h>
#include <stddef.h>

bool ProgTP_LoadCommandResult(int argc, char **argv, ProgTP_CommandResult *result, char *error, size_t error_size);

#endif
