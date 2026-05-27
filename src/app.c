#include "app.h"

#if !defined(CLAY_WASM) && !defined(PROGTP_WEB)
#include <stdio.h>
#endif

static uint32_t CStringLength(const char *value) {
    uint32_t length = 0;
    while (value[length] != '\0') {
        ++length;
    }
    return length;
}

static const Clay_Color COLOR_PAGE = {28, 31, 36, 255};
static const Clay_Color COLOR_PANEL = {245, 247, 250, 255};
static const Clay_Color COLOR_ACCENT = {36, 121, 108, 255};
static const Clay_Color COLOR_TEXT = {22, 26, 31, 255};
static const Clay_Color COLOR_MUTED = {91, 99, 110, 255};

void ProgTP_HandleClayError(Clay_ErrorData errorData) {
#if !defined(CLAY_WASM) && !defined(PROGTP_WEB)
    fprintf(stderr, "Clay error: %.*s\n", (int)errorData.errorText.length, errorData.errorText.chars);
#else
    (void)errorData;
#endif
}

Clay_RenderCommandArray ProgTP_BuildHelloWorldLayout(const char *target_name, float delta_time) {
    (void)delta_time;

    Clay_BeginLayout();

    CLAY(CLAY_ID("Root"), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .padding = CLAY_PADDING_ALL(24),
            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = COLOR_PAGE,
    }) {
        CLAY(CLAY_ID("Panel"), {
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = {
                    CLAY_SIZING_GROW(.max = 520),
                    CLAY_SIZING_FIT(0),
                },
                .padding = CLAY_PADDING_ALL(28),
                .childGap = 14,
            },
            .backgroundColor = COLOR_PANEL,
            .cornerRadius = CLAY_CORNER_RADIUS(8),
            .border = {
                .color = COLOR_ACCENT,
                .width = { 2, 2, 2, 2 },
            },
        }) {
            CLAY_TEXT(CLAY_STRING("Hello from Clay"), CLAY_TEXT_CONFIG({
                .fontSize = 30,
                .textColor = COLOR_TEXT,
            }));
            CLAY_TEXT(CLAY_STRING("Two shared C layout, three renderers."), CLAY_TEXT_CONFIG({
                .fontSize = 18,
                .textColor = COLOR_MUTED,
            }));
            CLAY(CLAY_ID("TargetBadge"), {
                .layout = {
                    .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
                    .padding = { 12, 12, 8, 8 },
                },
                .backgroundColor = COLOR_ACCENT,
                .cornerRadius = CLAY_CORNER_RADIUS(6),
            }) {
                Clay_String target = {
                    .length = CStringLength(target_name),
                    .chars = (char *)target_name,
                    .isStaticallyAllocated = true,
                };
                CLAY_TEXT(target, CLAY_TEXT_CONFIG({
                    .fontSize = 16,
                    .textColor = {255, 255, 255, 255},
                }));
            }
        }
    }

    return Clay_EndLayout(delta_time);
}
