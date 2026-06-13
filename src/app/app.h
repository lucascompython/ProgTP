#ifndef PROGTP_APP_H
#define PROGTP_APP_H

#include "connectivity.h"
#include "equipment_inventory.h"
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
    PROGTP_APP_ACTION_MODULE_7,
    PROGTP_APP_ACTION_MODULE_8,
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
void ProgTP_AppHandleAction(ProgTP_AppState *state, ProgTP_AppAction action);
void ProgTP_AppHandleTextInput(ProgTP_AppState *state, uint32_t codepoint);
Clay_RenderCommandArray ProgTP_AppBuildLayout(ProgTP_AppState *state, const char *target_name, float delta_time);

#endif
