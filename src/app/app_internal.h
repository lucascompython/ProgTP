#ifndef PROGTP_APP_INTERNAL_H
#define PROGTP_APP_INTERNAL_H

#include "app.h"

#include <clay.h>

#define PROGTP_UI_MODULE_BASE 100u
#define PROGTP_UI_SELECT_BASE 1000u
#define PROGTP_UI_SENSOR_SELECT_BASE 2000u
#define PROGTP_UI_INCIDENT_SELECT_BASE 5000u
#define PROGTP_UI_INCIDENT_FORM_FIELD_BASE 6000u
#define PROGTP_UI_CONFIG_SELECT_BASE 7000u
#define PROGTP_UI_FILES_SELECT_BASE 8000u
#define PROGTP_UI_FORM_FIELD_BASE 3000u
#define PROGTP_UI_FORM_STATE_BASE 4000u
#define PROGTP_VISIBLE_ROWS 12u

extern const Clay_Color COLOR_PAGE;
extern const Clay_Color COLOR_SIDEBAR;
extern const Clay_Color COLOR_SURFACE;
extern const Clay_Color COLOR_SURFACE_ALT;
extern const Clay_Color COLOR_SURFACE_DARK;
extern const Clay_Color COLOR_ACCENT;
extern const Clay_Color COLOR_ACCENT_DARK;
extern const Clay_Color COLOR_DANGER;
extern const Clay_Color COLOR_TEXT;
extern const Clay_Color COLOR_MUTED;
extern const Clay_Color COLOR_WHITE;
extern const Clay_Color COLOR_LINE;
extern const Clay_Color COLOR_OVERLAY;

extern ProgTP_AppState *progtp_interaction_state;
extern bool progtp_ui_compact;

float ControlHeight(void);
float ButtonMinWidth(void);
uint16_t ControlGap(void);

void SetStatus(ProgTP_AppState *state, const char *message);
void MarkInventoryChanged(ProgTP_AppState *state);
void EnsureSelection(ProgTP_AppState *state);
void EnsureSelectionInCurrentView(ProgTP_AppState *state);

const char *ModuleName(int module);
const char *InputModeName(ProgTP_AppInputMode mode);
const char *SearchPlaceholder(ProgTP_AppInputMode mode);

char *FormFieldBuffer(ProgTP_AppState *state, ProgTP_AppFormField field, size_t *buffer_size);
const char *FormFieldLabel(ProgTP_AppFormField field);
void AdvanceFormField(ProgTP_AppState *state, int direction);
void BackspaceFormField(ProgTP_AppState *state);
void SubmitEquipmentForm(ProgTP_AppState *state);
void ConfirmRemoveSelected(ProgTP_AppState *state);
void CloseModal(ProgTP_AppState *state);
void OpenSensorFileModal(ProgTP_AppState *state);
void SubmitSensorFile(ProgTP_AppState *state);
void OpenConfigFileModal(ProgTP_AppState *state);
void SubmitConfigFile(ProgTP_AppState *state);
void OpenRemoveConfigModal(ProgTP_AppState *state);
void ConfirmRemoveConfig(ProgTP_AppState *state);

bool MatchesCurrentView(const ProgTP_AppState *state, const ProgTP_Equipment *equipment);

void QueueConnectivityRequest(ProgTP_AppState *state, ProgTP_ConnectivityOperation operation);

void EnsureSensorSelection(ProgTP_AppState *state);
void EnsureSensorSelectionInFilter(ProgTP_AppState *state);
void QueueSensorImportRequest(ProgTP_AppState *state);
void MoveSensorSelection(ProgTP_AppState *state, int direction);
void PageSensors(ProgTP_AppState *state, int direction);

void MoveSelection(ProgTP_AppState *state, int direction);
void MoveInventorySelection(ProgTP_AppState *state, int direction);
void PageRows(ProgTP_AppState *state, int direction);
void SetStateFilter(ProgTP_AppState *state, bool enabled, ProgTP_EquipmentState equipment_state);
void SetTypeFilter(ProgTP_AppState *state, const char *type);
void CycleStateFilter(ProgTP_AppState *state, int direction);
void CycleTypeFilter(ProgTP_AppState *state, int direction);
void PrepareRows(ProgTP_AppState *state);
void InventoryModule(ProgTP_AppState *state);

bool HandleModalAction(ProgTP_AppState *state, ProgTP_AppAction action);
void OpenAddModal(ProgTP_AppState *state);
void OpenUpdateModal(ProgTP_AppState *state);
void OpenRemoveModal(ProgTP_AppState *state);
void ModalOverlay(ProgTP_AppState *state, Clay_Dimensions layout_dimensions);

void PrepareConnectivityText(ProgTP_AppState *state);
void ConnectivityModule(ProgTP_AppState *state);

void PrepareSensorText(ProgTP_AppState *state);
void SensorModule(ProgTP_AppState *state);

void PrepareIncidentText(ProgTP_AppState *state);
void MoveIncidentSelection(ProgTP_AppState *state, int direction);
void PageIncidents(ProgTP_AppState *state, int direction);
void IncidentModule(ProgTP_AppState *state);
void OpenAddIncidentModal(ProgTP_AppState *state);
void OpenUpdateIncidentModal(ProgTP_AppState *state);
void OpenRemoveIncidentModal(ProgTP_AppState *state);
void SubmitIncidentForm(ProgTP_AppState *state);
void ConfirmRemoveIncident(ProgTP_AppState *state);

void PrepareConfigText(ProgTP_AppState *state);
void MoveConfigSelection(ProgTP_AppState *state, int direction);
void PageConfig(ProgTP_AppState *state, int direction);
void ConfigModule(ProgTP_AppState *state);

void PrepareFilesText(ProgTP_AppState *state);
void MoveFilesSelection(ProgTP_AppState *state, int direction);
void PageFiles(ProgTP_AppState *state, int direction);
void FilesModule(ProgTP_AppState *state);
size_t ProgTP_AppGetFilesCount(void);

int32_t CStringLength(const char *value);
Clay_String StringFromCString(const char *value);

void ProgTP_AppHandleAction(ProgTP_AppState *state, ProgTP_AppAction action);

void TextLine(const char *text, uint16_t size, Clay_Color color);
void AttachInteraction(uintptr_t interaction);
void Button(uint32_t id, const char *label, uintptr_t interaction, bool active, bool danger);
void FormTextField(uint32_t id, ProgTP_AppState *state, ProgTP_AppFormField field);
void StateButton(uint32_t id, ProgTP_AppState *state, ProgTP_EquipmentState equipment_state);
void ModuleButton(ProgTP_AppState *state, int module);
void Metric(const char *label, const char *value);

void PlaceholderModule(int module);

#endif
