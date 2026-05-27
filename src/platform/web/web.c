#ifndef PROGTP_WEB
#define PROGTP_WEB
#endif
#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGTP_WEB_MEMORY_SIZE (10u * 1024u * 1024u)
#define PROGTP_WEB_MAX_COMMANDS 512u
#define PROGTP_WEB_LABEL_CAPACITY 192u

#if defined(__wasm__)
#define PROGTP_WASM_EXPORT(name) __attribute__((export_name(name)))
#else
#define PROGTP_WASM_EXPORT(name)
#endif

typedef struct {
    float x;
    float y;
    float width;
    float height;
    float r;
    float g;
    float b;
    float a;
    float radiusTopLeft;
    float radiusTopRight;
    float radiusBottomLeft;
    float radiusBottomRight;
    uint32_t textPointer;
    uint32_t textLength;
    uint32_t id;
    uint32_t fontSize;
    uint32_t borderLeft;
    uint32_t borderRight;
    uint32_t borderTop;
    uint32_t borderBottom;
    uint32_t type;
} ProgTP_WebRenderCommand;

static uint64_t progtp_web_memory[PROGTP_WEB_MEMORY_SIZE / sizeof(uint64_t)];
static bool progtp_web_initialized;
static ProgTP_WebRenderCommand progtp_web_commands[PROGTP_WEB_MAX_COMMANDS];
static char progtp_web_label[PROGTP_WEB_LABEL_CAPACITY] = "web: loading /api/hello";

void *memset(void *destination, int value, size_t length) {
    unsigned char *bytes = destination;
    for (size_t i = 0; i < length; ++i) {
        bytes[i] = (unsigned char)value;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t length) {
    unsigned char *dst = destination;
    const unsigned char *src = source;
    for (size_t i = 0; i < length; ++i) {
        dst[i] = src[i];
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t length) {
    unsigned char *dst = destination;
    const unsigned char *src = source;
    if (dst < src) {
        for (size_t i = 0; i < length; ++i) {
            dst[i] = src[i];
        }
    } else if (dst > src) {
        for (size_t i = length; i > 0; --i) {
            dst[i - 1] = src[i - 1];
        }
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t length) {
    const unsigned char *a = left;
    const unsigned char *b = right;
    for (size_t i = 0; i < length; ++i) {
        if (a[i] != b[i]) {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}

static Clay_Dimensions ProgTP_WebMeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    (void)userData;
    return (Clay_Dimensions){
        .width = (float)text.length * (float)config->fontSize * 0.52f,
        .height = (float)config->fontSize * 1.15f,
    };
}

static Clay_Vector2 ProgTP_WebQueryScrollOffset(uint32_t elementId, void *userData) {
    (void)elementId;
    (void)userData;
    return (Clay_Vector2){0};
}

static void ProgTP_WebInitialize(float width, float height) {
    if (progtp_web_initialized) {
        return;
    }

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(sizeof(progtp_web_memory), progtp_web_memory);
    Clay_Initialize(arena, (Clay_Dimensions){ width, height }, (Clay_ErrorHandler){ ProgTP_HandleClayError, NULL });
    Clay_SetMeasureTextFunction(ProgTP_WebMeasureText, NULL);
    Clay_SetQueryScrollOffsetFunction(ProgTP_WebQueryScrollOffset, NULL);
    progtp_web_initialized = true;
}

static void ProgTP_WebCopyBox(ProgTP_WebRenderCommand *output, Clay_BoundingBox box) {
    output->x = box.x;
    output->y = box.y;
    output->width = box.width;
    output->height = box.height;
}

static void ProgTP_WebCopyColor(ProgTP_WebRenderCommand *output, Clay_Color color) {
    output->r = color.r;
    output->g = color.g;
    output->b = color.b;
    output->a = color.a;
}

static void ProgTP_WebCopyRadius(ProgTP_WebRenderCommand *output, Clay_CornerRadius radius) {
    output->radiusTopLeft = radius.topLeft;
    output->radiusTopRight = radius.topRight;
    output->radiusBottomLeft = radius.bottomLeft;
    output->radiusBottomRight = radius.bottomRight;
}

static uint32_t ProgTP_WebBuildCommands(Clay_RenderCommandArray clay_commands) {
    uint32_t output_count = 0;
    for (int32_t i = 0; i < clay_commands.length && output_count < PROGTP_WEB_MAX_COMMANDS; ++i) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&clay_commands, i);
        ProgTP_WebRenderCommand *output = &progtp_web_commands[output_count];
        *output = (ProgTP_WebRenderCommand){0};
        output->type = (uint32_t)command->commandType;
        output->id = command->id;
        ProgTP_WebCopyBox(output, command->boundingBox);

        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                ProgTP_WebCopyColor(output, command->renderData.rectangle.backgroundColor);
                ProgTP_WebCopyRadius(output, command->renderData.rectangle.cornerRadius);
                ++output_count;
                break;

            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                ProgTP_WebCopyColor(output, command->renderData.border.color);
                ProgTP_WebCopyRadius(output, command->renderData.border.cornerRadius);
                output->borderLeft = command->renderData.border.width.left;
                output->borderRight = command->renderData.border.width.right;
                output->borderTop = command->renderData.border.width.top;
                output->borderBottom = command->renderData.border.width.bottom;
                ++output_count;
                break;

            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                ProgTP_WebCopyColor(output, command->renderData.text.textColor);
                output->textPointer = (uint32_t)(uintptr_t)command->renderData.text.stringContents.chars;
                output->textLength = (uint32_t)command->renderData.text.stringContents.length;
                output->fontSize = command->renderData.text.fontSize;
                ++output_count;
                break;

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                ++output_count;
                break;

            case CLAY_RENDER_COMMAND_TYPE_NONE:
            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                break;
        }
    }
    return output_count;
}

PROGTP_WASM_EXPORT("UpdateDrawFrame")
uint32_t UpdateDrawFrame(
    float width,
    float height,
    float mouse_wheel_x,
    float mouse_wheel_y,
    float mouse_position_x,
    float mouse_position_y,
    bool is_touch_down,
    bool is_mouse_down,
    float delta_time) {
    ProgTP_WebInitialize(width, height);
    Clay_SetLayoutDimensions((Clay_Dimensions){ width, height });
    Clay_SetPointerState((Clay_Vector2){ mouse_position_x, mouse_position_y }, is_touch_down || is_mouse_down);
    Clay_UpdateScrollContainers(is_touch_down, (Clay_Vector2){ mouse_wheel_x, mouse_wheel_y }, delta_time);

    Clay_RenderCommandArray clay_commands = ProgTP_BuildHelloWorldLayout(progtp_web_label, delta_time);
    return ProgTP_WebBuildCommands(clay_commands);
}

PROGTP_WASM_EXPORT("GetWebCommandBuffer")
uint32_t GetWebCommandBuffer(void) {
    return (uint32_t)(uintptr_t)progtp_web_commands;
}

PROGTP_WASM_EXPORT("GetWebCommandSize")
uint32_t GetWebCommandSize(void) {
    return (uint32_t)sizeof(ProgTP_WebRenderCommand);
}

PROGTP_WASM_EXPORT("GetCommandLabelBuffer")
uint32_t GetCommandLabelBuffer(void) {
    return (uint32_t)(uintptr_t)progtp_web_label;
}

PROGTP_WASM_EXPORT("GetCommandLabelCapacity")
uint32_t GetCommandLabelCapacity(void) {
    return PROGTP_WEB_LABEL_CAPACITY;
}

int main(void) {
    return 0;
}
