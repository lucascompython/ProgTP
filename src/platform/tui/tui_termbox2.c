#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "../../../subprojects/clay/renderers/termbox2/clay_renderer_termbox2.c"

#include "app.h"
#include "command_client.h"

#define TB_IMPL
#include <termbox2.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    ProgTP_CommandResult command_result;
    char command_error[256] = {0};
    if (!ProgTP_LoadCommandResult(argc, argv, &command_result, command_error, sizeof(command_error))) {
        ProgTP_RunLocalCommand(&command_result);
    }
    char command_label[192];
    ProgTP_FormatCommandResultLabel(&command_result, command_label, sizeof(command_label));

    Clay_Termbox_Initialize(
        TB_OUTPUT_256,
        CLAY_TB_BORDER_MODE_DEFAULT,
        CLAY_TB_BORDER_CHARS_DEFAULT,
        CLAY_TB_IMAGE_MODE_PLACEHOLDER,
        false);

    uint64_t clay_memory_size = Clay_MinMemorySize();
    Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, malloc(clay_memory_size));
    Clay_Initialize(clay_arena, (Clay_Dimensions){ Clay_Termbox_Width(), Clay_Termbox_Height() }, (Clay_ErrorHandler){ ProgTP_HandleClayError, NULL });
    Clay_SetMeasureTextFunction(Clay_Termbox_MeasureText, NULL);

    Clay_RenderCommandArray commands = ProgTP_BuildHelloWorldLayout(command_label, 0.0f);
    tb_clear();
    Clay_Termbox_Render(commands);
    tb_present();

    bool running = true;
    while (running) {
        struct tb_event event;
        int event_result = tb_poll_event(&event);
        if (event_result == TB_OK) {
            if (event.type == TB_EVENT_KEY && (event.key == TB_KEY_ESC || event.key == TB_KEY_CTRL_C || event.ch == 'q' || event.ch == 'Q')) {
                running = false;
            } else if (event.type == TB_EVENT_RESIZE) {
                Clay_SetLayoutDimensions((Clay_Dimensions){ Clay_Termbox_Width(), Clay_Termbox_Height() });
            }
        }

        Clay_SetLayoutDimensions((Clay_Dimensions){ Clay_Termbox_Width(), Clay_Termbox_Height() });
        Clay_SetPointerState((Clay_Vector2){ 0, 0 }, false);
        commands = ProgTP_BuildHelloWorldLayout(command_label, 0.016f);

        tb_clear();
        Clay_Termbox_Render(commands);
        tb_present();
    }

    Clay_Termbox_Close();
    free(clay_arena.memory);
    return 0;
}
