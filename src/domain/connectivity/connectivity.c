#include "connectivity.h"

#include "incident_store.h"
#include "progtp_error.h"
#include "progtp_text.h"
#include "progtp_time.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define PROGTP_POPEN _popen
#define PROGTP_PCLOSE _pclose
#else
#include <sys/wait.h>
#define PROGTP_POPEN popen
#define PROGTP_PCLOSE pclose
#endif

static bool IsSafeIpAddress(const char *value) {
    if (!value || value[0] == '\0') {
        return false;
    }
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor; ++cursor) {
        if (!isalnum(*cursor) && *cursor != '.' && *cursor != ':' && *cursor != '-' && *cursor != '%' && *cursor != '_') {
            return false;
        }
    }
    return true;
}

static void AppendPreview(char *preview, size_t preview_size, const char *data, size_t data_length) {
    size_t current = strlen(preview);
    if (current + 1u >= preview_size) {
        return;
    }
    size_t available = preview_size - current - 1u;
    size_t copy_length = data_length < available ? data_length : available;
    memcpy(preview + current, data, copy_length);
    preview[current + copy_length] = '\0';
}

static int NormalizeExitCode(int status) {
#if defined(_WIN32)
    return status;
#else
    if (status == -1) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return status;
#endif
}

static bool RunCommand(
    const char *command,
    FILE *output_file,
    char *preview,
    size_t preview_size,
    int *exit_code,
    char *error,
    size_t error_size) {
    char shell_command[PROGTP_CONNECTIVITY_COMMAND_SIZE + 16u];
    int written = snprintf(shell_command, sizeof(shell_command), "%s 2>&1", command);
    if (written < 0 || (size_t)written >= sizeof(shell_command)) {
        ProgTP_SetError(error, error_size, "command is too long");
        return false;
    }

    // NOLINTNEXTLINE(bugprone-command-processor): Module 2 explicitly supports user-entered custom shell commands.
    FILE *pipe = PROGTP_POPEN(shell_command, "r");
    if (!pipe) {
        char message[160];
        snprintf(message, sizeof(message), "could not start command: %s", strerror(errno));
        ProgTP_SetError(error, error_size, message);
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t length = strlen(buffer);
        if (fwrite(buffer, 1u, length, output_file) != length) {
            PROGTP_PCLOSE(pipe);
            ProgTP_SetError(error, error_size, "could not write command output");
            return false;
        }
        AppendPreview(preview, preview_size, buffer, length);
    }
    *exit_code = NormalizeExitCode(PROGTP_PCLOSE(pipe));
    return true;
}

static bool PingOutputIndicatesResponse(const char *output, int exit_code) {
    return exit_code == 0 &&
        (ProgTP_TextContainsIgnoreCase(output, "ttl=") ||
         ProgTP_TextContainsIgnoreCase(output, "bytes from") ||
         ProgTP_TextContainsIgnoreCase(output, "bytes=32") ||
         ProgTP_TextContainsIgnoreCase(output, "0% packet loss"));
}

static bool ReadFileSection(
    const char *path,
    long offset,
    char *buffer,
    size_t buffer_size,
    char *error,
    size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        ProgTP_SetError(error, error_size, "could not read command output file");
        return false;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        ProgTP_SetError(error, error_size, "could not seek command output file");
        return false;
    }
    size_t length = fread(buffer, 1u, buffer_size - 1u, file);
    buffer[length] = '\0';
    fclose(file);
    return true;
}

static bool BuildPingCommand(const char *ip_address, char *buffer, size_t buffer_size) {
    if (!IsSafeIpAddress(ip_address)) {
        return false;
    }
#if defined(_WIN32)
    int written = snprintf(buffer, buffer_size, "ping -n 4 %s", ip_address);
#else
    int written = snprintf(buffer, buffer_size, "ping -c 4 %s", ip_address);
#endif
    return written > 0 && (size_t)written < buffer_size;
}

static bool ResolveCustomCommand(
    const char *template_command,
    const ProgTP_Equipment *equipment,
    char *buffer,
    size_t buffer_size,
    char *error,
    size_t error_size) {
    const char *cursor = template_command;
    size_t output_length = 0;
    while (*cursor) {
        if (cursor[0] == '{' && cursor[1] == 'i' && cursor[2] == 'p' && cursor[3] == '}') {
            if (!equipment) {
                ProgTP_SetError(error, error_size, "custom command uses {ip} but no equipment is selected");
                return false;
            }
            size_t ip_length = strlen(equipment->ip_address);
            if (output_length + ip_length >= buffer_size) {
                ProgTP_SetError(error, error_size, "custom command is too long after {ip} replacement");
                return false;
            }
            memcpy(buffer + output_length, equipment->ip_address, ip_length);
            output_length += ip_length;
            cursor += 4;
        } else {
            if (output_length + 1u >= buffer_size) {
                ProgTP_SetError(error, error_size, "custom command is too long");
                return false;
            }
            buffer[output_length++] = *cursor++;
        }
    }
    buffer[output_length] = '\0';
    return true;
}

static bool AppendMonitoringLog(
    const char *path,
    const ProgTP_ConnectivityResult *result,
    const ProgTP_Equipment *equipment,
    const char *outcome,
    char *error,
    size_t error_size) {
    FILE *log = fopen(path, "a");
    if (!log) {
        ProgTP_SetError(error, error_size, "could not open monitoring log");
        return false;
    }
    int written = fprintf(
        log,
        "%s;%s;%u;%s;%s;%s;%d;%s\n",
        result->timestamp,
        result->command,
        equipment ? equipment->code : 0u,
        equipment ? equipment->name : "custom",
        equipment ? equipment->ip_address : "-",
        outcome,
        result->exit_code,
        result->output_path);
    bool closed = fclose(log) == 0;
    bool ok = written > 0 && closed;
    if (!ok) {
        ProgTP_SetError(error, error_size, "could not write monitoring log");
    }
    return ok;
}

static bool RunPing(
    ProgTP_Equipment *equipment,
    FILE *output_file,
    const char *output_path,
    const char *monitoring_log_path,
    const char *incident_path,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size) {
    char command[PROGTP_CONNECTIVITY_COMMAND_SIZE];
    if (!BuildPingCommand(equipment->ip_address, command, sizeof(command))) {
        ProgTP_SetError(error, error_size, "equipment has an invalid IP address");
        return false;
    }

    long output_offset = ftell(output_file);
    fprintf(output_file, "\n=== #%u %s (%s) ===\n$ %s\n", equipment->code, equipment->name, equipment->ip_address, command);
    fflush(output_file);
    output_offset = ftell(output_file);

    result->command[0] = '\0';
    snprintf(result->command, sizeof(result->command), "%s", command);
    result->equipment_code = equipment->code;
    int exit_code = -1;
    if (!RunCommand(command, output_file, result->output_preview, sizeof(result->output_preview), &exit_code, error, error_size)) {
        return false;
    }
    fflush(output_file);

    char raw_output[PROGTP_CONNECTIVITY_PREVIEW_SIZE];
    if (!ReadFileSection(output_path, output_offset, raw_output, sizeof(raw_output), error, error_size)) {
        return false;
    }
    bool responded = PingOutputIndicatesResponse(raw_output, exit_code);
    result->exit_code = exit_code;
    result->equipment_responded = responded;
    result->command_succeeded = exit_code == 0;
    ++result->executed_count;
    if (responded) {
        ++result->responded_count;
    } else {
        ++result->failed_count;
        equipment->state = PROGTP_EQUIPMENT_FAILED;
        equipment->has_pending_incidents = true;
        char incident_error[192] = {0};
        if (!ProgTP_IncidentStoreAppendPingFailure(
                incident_path,
                equipment,
                result->timestamp,
                incident_error,
                sizeof(incident_error))) {
            ProgTP_SetError(error, error_size, incident_error);
            return false;
        }
        result->incident_created = true;
    }
    ProgTP_FormatCurrentDate(equipment->last_checked, sizeof(equipment->last_checked));
    result->inventory_changed = true;

    if (!AppendMonitoringLog(
            monitoring_log_path,
            result,
            equipment,
            responded ? "RESPONDED" : "NO_RESPONSE",
            error,
            error_size)) {
        return false;
    }
    return true;
}

bool ProgTP_ConnectivityExecute(
    ProgTP_EquipmentInventory *inventory,
    const ProgTP_ConnectivityRequest *request,
    const char *ping_output_path,
    const char *custom_output_path,
    const char *monitoring_log_path,
    const char *incident_path,
    ProgTP_ConnectivityResult *result,
    char *error,
    size_t error_size) {
    if (!inventory || !request || !result) {
        ProgTP_SetError(error, error_size, "missing connectivity request data");
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->exit_code = -1;
    ProgTP_FormatCurrentTimestamp(result->timestamp, sizeof(result->timestamp));

    if (request->operation == PROGTP_CONNECTIVITY_CUSTOM) {
        if (request->custom_command[0] == '\0') {
            ProgTP_SetError(error, error_size, "custom command cannot be empty");
            return false;
        }
        const ProgTP_Equipment *equipment = request->equipment_code == 0
            ? NULL
            : ProgTP_EquipmentInventoryFindByCodeConst(inventory, request->equipment_code);
        result->equipment_code = request->equipment_code;
        if (!ResolveCustomCommand(
                request->custom_command,
                equipment,
                result->command,
                sizeof(result->command),
                error,
                error_size)) {
            return false;
        }
        snprintf(result->output_path, sizeof(result->output_path), "%s", custom_output_path);
        FILE *output = fopen(custom_output_path, "wb");
        if (!output) {
            ProgTP_SetError(error, error_size, "could not create custom command output file");
            return false;
        }
        bool ran = RunCommand(
            result->command,
            output,
            result->output_preview,
            sizeof(result->output_preview),
            &result->exit_code,
            error,
            error_size);
        bool closed = fclose(output) == 0;
        if (!ran || !closed) {
            if (ran) {
                ProgTP_SetError(error, error_size, "could not close custom command output file");
            }
            return false;
        }
        result->executed_count = 1u;
        result->command_succeeded = result->exit_code == 0;
        snprintf(
            result->summary,
            sizeof(result->summary),
            "Custom command %s with exit code %d",
            result->command_succeeded ? "completed" : "failed",
            result->exit_code);
        return AppendMonitoringLog(
            monitoring_log_path,
            result,
            equipment,
            result->command_succeeded ? "COMPLETED" : "FAILED",
            error,
            error_size);
    }

    snprintf(result->output_path, sizeof(result->output_path), "%s", ping_output_path);
    FILE *output = fopen(ping_output_path, "wb");
    if (!output) {
        ProgTP_SetError(error, error_size, "could not create ping output file");
        return false;
    }

    bool ok = true;
    if (request->operation == PROGTP_CONNECTIVITY_PING_SELECTED) {
        ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryFindByCode(inventory, request->equipment_code);
        if (!equipment) {
            ProgTP_SetError(error, error_size, "selected equipment was not found");
            ok = false;
        } else {
            ok = RunPing(
                equipment,
                output,
                ping_output_path,
                monitoring_log_path,
                incident_path,
                result,
                error,
                error_size);
        }
    } else {
        size_t count = ProgTP_EquipmentInventoryGetCount(inventory);
        for (size_t i = 0; i < count && ok; ++i) {
            const ProgTP_Equipment *equipment = ProgTP_EquipmentInventoryGetByIndex(inventory, i);
            ok = RunPing(
                equipment,
                output,
                ping_output_path,
                monitoring_log_path,
                incident_path,
                result,
                error,
                error_size);
        }
    }

    if (fclose(output) != 0 && ok) {
        ProgTP_SetError(error, error_size, "could not close ping output file");
        ok = false;
    }
    if (!ok) {
        return false;
    }

    result->command_succeeded = result->failed_count == 0;
    result->equipment_responded = result->responded_count > 0 && result->failed_count == 0;
    if (request->operation == PROGTP_CONNECTIVITY_PING_SELECTED) {
        snprintf(
            result->summary,
            sizeof(result->summary),
            "Equipment #%u %s; raw output saved to %s%s",
            result->equipment_code,
            result->equipment_responded ? "responded" : "did not respond",
            result->output_path,
            result->incident_created ? "; pending incident created" : "");
    } else {
        snprintf(
            result->summary,
            sizeof(result->summary),
            "Network test complete: %u responded, %u failed; raw output saved to %s",
            result->responded_count,
            result->failed_count,
            result->output_path);
    }
    return true;
}
