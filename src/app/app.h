#ifndef PROGTP_APP_H
#define PROGTP_APP_H

#include "config_history.h"
#include "connectivity.h"
#include "equipment_inventory.h"
#include "incident_store.h"
#include "protocol.h"
#include "sensor_store.h"

#include <clay.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    PROGTP_APP_ACTION_NONE = 0,
    PROGTP_APP_ACTION_NEXT,
    PROGTP_APP_ACTION_PREVIOUS,
    PROGTP_APP_ACTION_ADD_SAMPLE,
    PROGTP_APP_ACTION_UPDATE_SELECTED,
    PROGTP_APP_ACTION_REMOVE_SELECTED,
    PROGTP_APP_ACTION_CYCLE_STATE,
    PROGTP_APP_ACTION_TOGGLE_PENDING,
    PROGTP_APP_ACTION_FILTER_ALL,
    PROGTP_APP_ACTION_FILTER_ROUTERS,
    PROGTP_APP_ACTION_FILTER_FAILED,
    PROGTP_APP_ACTION_FILTER_PENDING,
    PROGTP_APP_ACTION_SEARCH_CODE,
    PROGTP_APP_ACTION_SEARCH_IP,
    PROGTP_APP_ACTION_SEARCH_MAC,
    PROGTP_APP_ACTION_SEARCH_FIELD,
    PROGTP_APP_ACTION_INPUT_BACKSPACE,
    PROGTP_APP_ACTION_INPUT_SUBMIT,
    PROGTP_APP_ACTION_SAVE,
    PROGTP_APP_ACTION_LOAD,
    PROGTP_APP_ACTION_MODULE_1,
    PROGTP_APP_ACTION_MODULE_2,
    PROGTP_APP_ACTION_MODULE_3,
    PROGTP_APP_ACTION_MODULE_4,
    PROGTP_APP_ACTION_MODULE_5,
    PROGTP_APP_ACTION_MODULE_6,
    PROGTP_APP_ACTION_FORM_SUBMIT,
    PROGTP_APP_ACTION_FORM_CANCEL,
    PROGTP_APP_ACTION_FORM_NEXT_FIELD,
    PROGTP_APP_ACTION_FORM_PREVIOUS_FIELD,
    PROGTP_APP_ACTION_PAGE_PREVIOUS,
    PROGTP_APP_ACTION_PAGE_NEXT,
    PROGTP_APP_ACTION_FILTER_STATE_ALL,
    PROGTP_APP_ACTION_FILTER_STATE_OPERATIONAL,
    PROGTP_APP_ACTION_FILTER_STATE_FAILED,
    PROGTP_APP_ACTION_FILTER_STATE_MAINTENANCE,
    PROGTP_APP_ACTION_FILTER_STATE_DISABLED,
    PROGTP_APP_ACTION_FILTER_TYPE_ALL,
    PROGTP_APP_ACTION_FILTER_TYPE_PREVIOUS,
    PROGTP_APP_ACTION_FILTER_TYPE_NEXT,
    PROGTP_APP_ACTION_FORM_TOGGLE_PENDING,
    PROGTP_APP_ACTION_FORM_STATE_PREVIOUS,
    PROGTP_APP_ACTION_FORM_STATE_NEXT,
    PROGTP_APP_ACTION_FILTER_STATE_PREVIOUS,
    PROGTP_APP_ACTION_FILTER_STATE_NEXT,
    PROGTP_APP_ACTION_CONNECTIVITY_PING_SELECTED,
    PROGTP_APP_ACTION_CONNECTIVITY_PING_ALL,
    PROGTP_APP_ACTION_CONNECTIVITY_COMMAND_FIELD,
    PROGTP_APP_ACTION_CONNECTIVITY_RUN_CUSTOM,
    PROGTP_APP_ACTION_CONNECTIVITY_PREVIOUS_TARGET,
    PROGTP_APP_ACTION_CONNECTIVITY_NEXT_TARGET,
    PROGTP_APP_ACTION_CONNECTIVITY_PAGE_PREVIOUS,
    PROGTP_APP_ACTION_CONNECTIVITY_PAGE_NEXT,
    PROGTP_APP_ACTION_SENSOR_IMPORT,
    PROGTP_APP_ACTION_SENSOR_PREVIOUS,
    PROGTP_APP_ACTION_SENSOR_NEXT,
    PROGTP_APP_ACTION_SENSOR_PAGE_PREVIOUS,
    PROGTP_APP_ACTION_SENSOR_PAGE_NEXT,
    PROGTP_APP_ACTION_SENSOR_FILTER_ALL,
    PROGTP_APP_ACTION_SENSOR_FILTER_ANOMALOUS,
    PROGTP_APP_ACTION_SENSOR_SEARCH_FIELD,
    PROGTP_APP_ACTION_SENSOR_CHOOSE_FILE,
    PROGTP_APP_ACTION_SENSOR_FETCH_API,
    PROGTP_APP_ACTION_INCIDENT_PREVIOUS,
    PROGTP_APP_ACTION_INCIDENT_NEXT,
    PROGTP_APP_ACTION_INCIDENT_PAGE_PREVIOUS,
    PROGTP_APP_ACTION_INCIDENT_PAGE_NEXT,
    PROGTP_APP_ACTION_INCIDENT_FILTER_ALL,
    PROGTP_APP_ACTION_INCIDENT_FILTER_PENDING,
    PROGTP_APP_ACTION_INCIDENT_FILTER_IN_PROGRESS,
    PROGTP_APP_ACTION_INCIDENT_FILTER_COMPLETED,
    PROGTP_APP_ACTION_INCIDENT_SORT_BY_ID,
    PROGTP_APP_ACTION_INCIDENT_SORT_BY_PRIORITY,
    PROGTP_APP_ACTION_INCIDENT_ADD,
    PROGTP_APP_ACTION_INCIDENT_EDIT,
    PROGTP_APP_ACTION_INCIDENT_DELETE,
    PROGTP_APP_ACTION_INCIDENT_START,
    PROGTP_APP_ACTION_INCIDENT_COMPLETE,
    PROGTP_APP_ACTION_INCIDENT_AUTO_IMPORT,
    PROGTP_APP_ACTION_INCIDENT_PRIORITY_LOW,
    PROGTP_APP_ACTION_INCIDENT_PRIORITY_MEDIUM,
    PROGTP_APP_ACTION_INCIDENT_PRIORITY_HIGH,
    PROGTP_APP_ACTION_CONFIG_PREVIOUS,
    PROGTP_APP_ACTION_CONFIG_NEXT,
    PROGTP_APP_ACTION_CONFIG_PAGE_PREVIOUS,
    PROGTP_APP_ACTION_CONFIG_PAGE_NEXT,
    PROGTP_APP_ACTION_CONFIG_UNDO,
    PROGTP_APP_ACTION_CONFIG_REDO,
    PROGTP_APP_ACTION_CONFIG_FILTER_ALL,
    PROGTP_APP_ACTION_CONFIG_FILTER_UNDONE,
    PROGTP_APP_ACTION_CONFIG_IMPORT,
    PROGTP_APP_ACTION_CONFIG_DELETE,
    PROGTP_APP_ACTION_FILES_REFRESH,
    PROGTP_APP_ACTION_FILES_PREVIOUS,
    PROGTP_APP_ACTION_FILES_NEXT,
    PROGTP_APP_ACTION_FILES_PAGE_PREVIOUS,
    PROGTP_APP_ACTION_FILES_PAGE_NEXT,
    PROGTP_APP_ACTION_FILES_FILTER_ALL,
    PROGTP_APP_ACTION_FILES_FILTER_BINARY,
    PROGTP_APP_ACTION_FILES_FILTER_TEXT,
    PROGTP_APP_ACTION_FILES_GENERATE_NETWORK_REPORT,
    PROGTP_APP_ACTION_FILES_GENERATE_INCIDENT_REPORT,
} ProgTP_AppAction;

typedef enum {
    PROGTP_APP_VIEW_ALL = 0,
    PROGTP_APP_VIEW_ROUTERS,
    PROGTP_APP_VIEW_FAILED,
    PROGTP_APP_VIEW_PENDING,
} ProgTP_AppView;

typedef enum {
    PROGTP_APP_INPUT_NONE = 0,
    PROGTP_APP_INPUT_SEARCH_CODE,
    PROGTP_APP_INPUT_SEARCH_IP,
    PROGTP_APP_INPUT_SEARCH_MAC,
    PROGTP_APP_INPUT_CONNECTIVITY_COMMAND,
    PROGTP_APP_INPUT_SENSOR_CODE,
} ProgTP_AppInputMode;

typedef enum {
    PROGTP_APP_MODAL_NONE = 0,
    PROGTP_APP_MODAL_ADD_EQUIPMENT,
    PROGTP_APP_MODAL_UPDATE_EQUIPMENT,
    PROGTP_APP_MODAL_REMOVE_EQUIPMENT,
    PROGTP_APP_MODAL_SENSOR_FILE,
    PROGTP_APP_MODAL_ADD_INCIDENT,
    PROGTP_APP_MODAL_UPDATE_INCIDENT,
    PROGTP_APP_MODAL_REMOVE_INCIDENT,
    PROGTP_APP_MODAL_CONFIG_FILE,
    PROGTP_APP_MODAL_REMOVE_CONFIG,
} ProgTP_AppModal;

typedef enum {
    PROGTP_APP_FORM_NAME = 0,
    PROGTP_APP_FORM_TYPE,
    PROGTP_APP_FORM_BRAND,
    PROGTP_APP_FORM_MODEL,
    PROGTP_APP_FORM_IP,
    PROGTP_APP_FORM_MAC,
    PROGTP_APP_FORM_LOCATION,
    PROGTP_APP_FORM_FIELD_COUNT,
} ProgTP_AppFormField;

typedef enum {
    PROGTP_APP_INCIDENT_FORM_EQUIPMENT_CODE = 0,
    PROGTP_APP_INCIDENT_FORM_SOURCE,
    PROGTP_APP_INCIDENT_FORM_TYPE,
    PROGTP_APP_INCIDENT_FORM_DESCRIPTION,
    PROGTP_APP_INCIDENT_FORM_PRIORITY,
    PROGTP_APP_INCIDENT_FORM_TECHNICIAN,
    PROGTP_APP_INCIDENT_FORM_FIELD_COUNT,
} ProgTP_AppIncidentFormField;

typedef struct {
    ProgTP_EquipmentInventory inventory;
    ProgTP_SensorStore sensors;
    uint32_t selected_code;
    size_t selected_sensor_index;
    int active_module;
    ProgTP_AppView view;
    ProgTP_AppInputMode input_mode;
    ProgTP_AppModal modal;
    ProgTP_AppFormField form_field;
    ProgTP_EquipmentState form_state;
    ProgTP_EquipmentState state_filter;
    bool persistence_enabled;
    bool terminal_rendering;
    bool inventory_dirty;
    bool state_filter_enabled;
    bool form_pending;
    bool connectivity_request_pending;
    bool connectivity_request_in_flight;
    bool connectivity_has_result;
    bool sensor_import_request_pending;
    bool sensor_import_request_in_flight;
    bool sensor_filter_anomalous;
    bool sensor_has_import_result;
    bool sensor_api_fetch_request_pending;
    bool sensor_api_fetch_in_flight;
    uint64_t inventory_version;
    char storage_path[128];
    char status[320];
    char input_text[64];
    char summary_text[256];
    char title_text[128];
    char module_labels[8][96];
    char total_metric_text[32];
    char selected_metric_text[32];
    char filter_metric_text[96];
    char type_filter_text[64];
    char selected_text[512];
    char help_text[384];
    char search_display_text[96];
    char form_display_text[PROGTP_APP_FORM_FIELD_COUNT][PROGTP_EQUIPMENT_NAME_SIZE + 2u];
    char row_page_text[96];
    char row_texts[12][512];
    uint32_t row_codes[12];
    size_t row_count;
    size_t row_offset;
    size_t filtered_count;
    char modal_title[96];
    char modal_message[256];
    char form_name[PROGTP_EQUIPMENT_NAME_SIZE];
    char form_type[PROGTP_EQUIPMENT_TYPE_SIZE];
    char form_brand[PROGTP_EQUIPMENT_BRAND_SIZE];
    char form_model[PROGTP_EQUIPMENT_MODEL_SIZE];
    char form_ip[PROGTP_EQUIPMENT_IP_SIZE];
    char form_mac[PROGTP_EQUIPMENT_MAC_SIZE];
    char form_location[PROGTP_EQUIPMENT_LOCATION_SIZE];
    char type_filter[PROGTP_EQUIPMENT_TYPE_SIZE];
    ProgTP_ConnectivityRequest connectivity_request;
    ProgTP_ConnectivityResult connectivity_result;
    char connectivity_custom_command[PROGTP_CONNECTIVITY_COMMAND_SIZE];
    char connectivity_command_display[PROGTP_CONNECTIVITY_COMMAND_SIZE + 2u];
    char connectivity_target_text[160];
    char connectivity_status_text[64];
    char connectivity_counts_text[96];
    char connectivity_row_page_text[96];
    char connectivity_row_texts[12][256];
    uint32_t connectivity_row_codes[12];
    size_t connectivity_row_count;
    size_t connectivity_row_offset;
    char connectivity_output_lines[8][192];
    size_t connectivity_output_line_count;
    ProgTP_SensorImportResult sensor_import_result;
    char sensor_metric_total_text[32];
    char sensor_metric_anomaly_text[32];
    char sensor_metric_selected_text[64];
    char sensor_search_text[64];
    char sensor_search_display[80];
    char sensor_row_page_text[96];
    char sensor_row_texts[12][256];
    size_t sensor_row_indices[12];
    size_t sensor_row_count;
    size_t sensor_row_offset;
    char sensor_selected_text[320];
    char sensor_input_path[512];
    char config_input_path[512];
    ProgTP_IncidentStore incidents;
    size_t selected_incident_index;
    uint32_t incident_filter_state;
    uint32_t incident_sort_mode;
    size_t incident_sorted_indices[128];
    size_t incident_sorted_count;
    bool incident_operation_pending;
    bool incident_operation_in_flight;
    bool incident_has_result;
    bool needs_incident_reload;
    ProgTP_AppIncidentFormField incident_form_field;
    char incident_form_equipment_code[16];
    char incident_form_source[PROGTP_EQUIPMENT_NAME_SIZE];
    char incident_form_type[PROGTP_INCIDENT_TYPE_SIZE];
    char incident_form_description[PROGTP_INCIDENT_DESCRIPTION_SIZE];
    char incident_form_priority[PROGTP_INCIDENT_PRIORITY_SIZE];
    char incident_form_technician[PROGTP_INCIDENT_TECHNICIAN_SIZE];
    char incident_metric_total_text[32];
    char incident_metric_pending_text[32];
    char incident_metric_in_progress_text[32];
    char incident_metric_completed_text[32];
    char incident_selected_text[512];
    char incident_detail_texts[5][256];
    size_t incident_detail_count;
    char incident_row_page_text[96];
    char incident_row_texts[12][256];
    uint32_t incident_row_numbers[12];
    size_t incident_row_count;
    size_t incident_row_offset;
    char incident_form_display_text[PROGTP_APP_INCIDENT_FORM_FIELD_COUNT][PROGTP_INCIDENT_DESCRIPTION_SIZE + 2u];
    ProgTP_IncidentOperationResponse incident_operation_result;
    ProgTP_IncidentOperationRequest pending_incident_operation;
    ProgTP_ConfigHistory config_history;
    size_t selected_config_index;
    uint32_t config_filter_state;
    bool config_operation_pending;
    bool config_operation_in_flight;
    ProgTP_ConfigOperationRequest pending_config_operation;
    ProgTP_ConfigOperationResponse config_operation_result;
    char config_metric_total_text[32];
    char config_metric_applied_text[32];
    char config_metric_undone_text[32];
    char config_selected_text[640];
    char config_row_page_text[96];
    char config_row_texts[12][256];
    size_t config_row_count;
    size_t config_row_offset;
    char files_metric_total_text[32];
    char files_metric_selected_text[64];
    char files_selected_text[640];
    char files_row_texts[12][256];
    char files_row_page_text[96];
    size_t files_row_count;
    size_t files_row_offset;
    size_t files_selected_index;
    uint32_t files_filter_state;
    char files_preview_lines[128][256];
    size_t files_preview_line_count;
    bool files_needs_refresh;
    bool files_preview_loaded;
} ProgTP_AppState;

void ProgTP_HandleClayError(Clay_ErrorData errorData);
void ProgTP_AppInit(ProgTP_AppState *state, bool persistence_enabled, const char *storage_path);
void ProgTP_AppDestroy(ProgTP_AppState *state);
void ProgTP_AppUseLoadedInventory(ProgTP_AppState *state, const char *status);
void ProgTP_AppSetStatus(ProgTP_AppState *state, const char *status);
void ProgTP_AppSetTerminalRendering(ProgTP_AppState *state, bool enabled);
bool ProgTP_AppInventoryDirty(const ProgTP_AppState *state);
uint64_t ProgTP_AppInventoryVersion(const ProgTP_AppState *state);
void ProgTP_AppMarkInventoryClean(ProgTP_AppState *state);
bool ProgTP_AppModalActive(const ProgTP_AppState *state);
void ProgTP_AppUseLoadedSensors(ProgTP_AppState *state, const ProgTP_SensorStore *store, const char *status);
bool ProgTP_AppTakeConnectivityRequest(ProgTP_AppState *state, ProgTP_ConnectivityRequest *request);
bool ProgTP_AppTakeSensorImportRequest(ProgTP_AppState *state);
void ProgTP_AppCompleteConnectivityRequest(
    ProgTP_AppState *state,
    const ProgTP_ConnectivityResult *result,
    bool inventory_changed_locally);
void ProgTP_AppCompleteSensorImport(
    ProgTP_AppState *state,
    const ProgTP_SensorImportResult *result,
    const ProgTP_SensorStore *store);
void ProgTP_AppFailConnectivityRequest(ProgTP_AppState *state, const char *error);
void ProgTP_AppFailSensorImport(ProgTP_AppState *state, const char *error);
bool ProgTP_AppTakeSensorApiFetchRequest(ProgTP_AppState *state);
void ProgTP_AppCompleteSensorApiFetch(
    ProgTP_AppState *state,
    const ProgTP_SensorImportResult *result,
    const ProgTP_SensorStore *store);
void ProgTP_AppFailSensorApiFetch(ProgTP_AppState *state, const char *error);
bool ProgTP_AppTakeIncidentOperationRequest(ProgTP_AppState *state, ProgTP_IncidentOperationRequest *request);
void ProgTP_AppCompleteIncidentOperation(ProgTP_AppState *state, const ProgTP_IncidentOperationResponse *response);
void ProgTP_AppFailIncidentOperation(ProgTP_AppState *state, const char *error);
void ProgTP_AppUseLoadedIncidents(ProgTP_AppState *state, const ProgTP_IncidentStore *store, const char *status);
bool ProgTP_AppIncidentOperationPending(const ProgTP_AppState *state);
bool ProgTP_AppIncidentOperationInFlight(const ProgTP_AppState *state);
bool ProgTP_AppTakeConfigOperationRequest(ProgTP_AppState *state, ProgTP_ConfigOperationRequest *request);
void ProgTP_AppCompleteConfigOperation(ProgTP_AppState *state, const ProgTP_ConfigOperationResponse *response);
void ProgTP_AppFailConfigOperation(ProgTP_AppState *state, const char *error);
void ProgTP_AppQueueConfigOperation(
    ProgTP_AppState *state,
    const ProgTP_ConfigOperationRequest *request);
void ProgTP_AppRecordConfigChange(
    ProgTP_AppState *state,
    ProgTP_ConfigOpType op_type,
    const ProgTP_Equipment *before,
    const ProgTP_Equipment *after,
    const char *description);
void ProgTP_AppHandleAction(ProgTP_AppState *state, ProgTP_AppAction action);
void ProgTP_AppHandleTextInput(ProgTP_AppState *state, uint32_t codepoint);
Clay_RenderCommandArray ProgTP_AppBuildLayout(ProgTP_AppState *state, const char *target_name, float delta_time);

#endif
