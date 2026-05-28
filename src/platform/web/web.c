#ifndef PROGTP_WEB
#define PROGTP_WEB
#endif
#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "app.h"

#include <stdbool.h>
#include <stdint.h>

#define PROGTP_WEB_LABEL_CAPACITY 192u

static char progtp_web_label[PROGTP_WEB_LABEL_CAPACITY] = "web: loading /api/hello";

CLAY_WASM_EXPORT("UpdateDrawFrame")
Clay_RenderCommandArray UpdateDrawFrame(
    float width,
    float height,
    float mouse_wheel_x,
    float mouse_wheel_y,
    float mouse_position_x,
    float mouse_position_y,
    bool is_touch_down,
    bool is_mouse_down,
    float delta_time) {
    Clay_SetLayoutDimensions((Clay_Dimensions){ width, height });
    Clay_SetPointerState((Clay_Vector2){ mouse_position_x, mouse_position_y }, is_touch_down || is_mouse_down);
    Clay_UpdateScrollContainers(is_touch_down, (Clay_Vector2){ mouse_wheel_x, mouse_wheel_y }, delta_time);

    return ProgTP_BuildHelloWorldLayout(progtp_web_label, delta_time);
}

CLAY_WASM_EXPORT("GetCommandLabelBuffer")
uint32_t GetCommandLabelBuffer(void) {
    return (uint32_t)(uintptr_t)progtp_web_label;
}

CLAY_WASM_EXPORT("GetCommandLabelCapacity")
uint32_t GetCommandLabelCapacity(void) {
    return PROGTP_WEB_LABEL_CAPACITY;
}
