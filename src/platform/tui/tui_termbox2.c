#define CLAY_IMPLEMENTATION
#include <clay.h>

#define TB_OPT_ATTR_W 32
#define TB_IMPL
#include <termbox2.h>

#include "app.h"
#include "clay_renderer_termbox2_basic.h"
#include "command_client.h"

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

    int termbox_result = ProgTP_Termbox_Initialize();
    if (termbox_result != TB_OK) {
        fprintf(stderr, "termbox2 init failed: %s\n", tb_strerror(termbox_result));
        return 1;
    }

    uint64_t clay_memory_size = Clay_MinMemorySize();
    Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, malloc(clay_memory_size));
    Clay_Initialize(clay_arena, (Clay_Dimensions){ ProgTP_Termbox_Width(), ProgTP_Termbox_Height() }, (Clay_ErrorHandler){ ProgTP_HandleClayError, NULL });
    Clay_SetMeasureTextFunction(ProgTP_Termbox_MeasureText, NULL);

    Clay_RenderCommandArray commands = ProgTP_BuildHelloWorldLayout(command_label, 0.0f);
    tb_clear();
    ProgTP_Termbox_Render(commands);
    tb_present();

    bool running = true;
    while (running) {
        struct tb_event event;
        int event_result = tb_poll_event(&event);
        if (event_result == TB_OK) {
            if (event.type == TB_EVENT_KEY && (event.key == TB_KEY_ESC || event.key == TB_KEY_CTRL_C || event.ch == 'q' || event.ch == 'Q')) {
                running = false;
            } else if (event.type == TB_EVENT_RESIZE) {
                Clay_SetLayoutDimensions((Clay_Dimensions){ ProgTP_Termbox_Width(), ProgTP_Termbox_Height() });
            }
        }

        Clay_SetLayoutDimensions((Clay_Dimensions){ ProgTP_Termbox_Width(), ProgTP_Termbox_Height() });
        Clay_SetPointerState((Clay_Vector2){ 0, 0 }, false);
        commands = ProgTP_BuildHelloWorldLayout(command_label, 0.016f);

        tb_clear();
        ProgTP_Termbox_Render(commands);
        tb_present();
    }

    ProgTP_Termbox_Close();
    free(clay_arena.memory);
    return 0;
}
