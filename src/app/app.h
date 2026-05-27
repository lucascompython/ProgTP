#ifndef PROGTP_APP_H
#define PROGTP_APP_H

#include <clay.h>

void ProgTP_HandleClayError(Clay_ErrorData errorData);
Clay_RenderCommandArray ProgTP_BuildHelloWorldLayout(const char *target_name, float delta_time);

#endif
