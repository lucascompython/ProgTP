#ifndef PROGTP_COMMAND_CLIENT_H
#define PROGTP_COMMAND_CLIENT_H

#include "protocol.h"

#include <stdbool.h>
#include <stddef.h>

const char *ProgTP_FindRemoteUrl(int argc, char **argv);
bool ProgTP_LoadCommandResult(int argc, char **argv, ProgTP_CommandResult *result, char *error, size_t error_size);
bool ProgTP_LoadRemoteInventory(const char *remote_url, ProgTP_EquipmentInventory *inventory, char *error, size_t error_size);
bool ProgTP_SaveRemoteInventory(const char *remote_url, const ProgTP_EquipmentInventory *inventory, char *error, size_t error_size);

#endif
