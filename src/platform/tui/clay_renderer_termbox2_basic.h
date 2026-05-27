#ifndef PROGTP_CLAY_RENDERER_TERMBOX2_BASIC_H
#define PROGTP_CLAY_RENDERER_TERMBOX2_BASIC_H

#include <clay.h>

#ifndef TERMBOX_H_INCL
#include <termbox2.h>
#endif

#include <stdint.h>
#include <stdlib.h>

#define PROGTP_TB_CELL_WIDTH 8.0f
#define PROGTP_TB_CELL_HEIGHT 16.0f
#define PROGTP_TB_KEEP_BACKGROUND ((uintattr_t)UINT32_MAX)

typedef struct {
    int x;
    int y;
    int width;
    int height;
} ProgTP_TermboxBox;

static uintattr_t *progtp_tb_backgrounds;
static int progtp_tb_background_width;
static int progtp_tb_background_height;
static bool progtp_tb_scissor_enabled;
static ProgTP_TermboxBox progtp_tb_scissor;

static int ProgTP_Termbox_Initialize(void) {
    int result = tb_init();
    if (result == TB_OK) {
        tb_set_output_mode(TB_OUTPUT_256);
        tb_set_input_mode(TB_INPUT_ESC);
    }
    return result;
}

static void ProgTP_Termbox_Close(void) {
    free(progtp_tb_backgrounds);
    progtp_tb_backgrounds = NULL;
    progtp_tb_background_width = 0;
    progtp_tb_background_height = 0;
    tb_shutdown();
}

static float ProgTP_Termbox_Width(void) {
    return (float)tb_width() * PROGTP_TB_CELL_WIDTH;
}

static float ProgTP_Termbox_Height(void) {
    return (float)tb_height() * PROGTP_TB_CELL_HEIGHT;
}

static Clay_Dimensions ProgTP_Termbox_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    (void)config;
    (void)userData;
    return (Clay_Dimensions){
        .width = (float)text.length * PROGTP_TB_CELL_WIDTH,
        .height = PROGTP_TB_CELL_HEIGHT,
    };
}

static int ProgTP_Termbox_FloorCell(float value, float cell_size) {
    return (int)(value / cell_size);
}

static int ProgTP_Termbox_CeilCell(float value, float cell_size) {
    int cell = (int)(value / cell_size);
    if ((float)cell * cell_size < value) {
        ++cell;
    }
    return cell;
}

static ProgTP_TermboxBox ProgTP_Termbox_BoxFromClay(Clay_BoundingBox box) {
    int x0 = ProgTP_Termbox_FloorCell(box.x, PROGTP_TB_CELL_WIDTH);
    int y0 = ProgTP_Termbox_FloorCell(box.y, PROGTP_TB_CELL_HEIGHT);
    int x1 = ProgTP_Termbox_CeilCell(box.x + box.width, PROGTP_TB_CELL_WIDTH);
    int y1 = ProgTP_Termbox_CeilCell(box.y + box.height, PROGTP_TB_CELL_HEIGHT);
    return (ProgTP_TermboxBox){
        .x = x0,
        .y = y0,
        .width = x1 - x0,
        .height = y1 - y0,
    };
}

static uintattr_t ProgTP_Termbox_Color(Clay_Color color) {
    if (color.a <= 0.0f) {
        return TB_DEFAULT;
    }

    int red = (int)((color.r / 255.0f) * 5.0f + 0.5f);
    int green = (int)((color.g / 255.0f) * 5.0f + 0.5f);
    int blue = (int)((color.b / 255.0f) * 5.0f + 0.5f);
    if (red < 0) red = 0;
    if (green < 0) green = 0;
    if (blue < 0) blue = 0;
    if (red > 5) red = 5;
    if (green > 5) green = 5;
    if (blue > 5) blue = 5;
    return (uintattr_t)(16 + (36 * red) + (6 * green) + blue);
}

static bool ProgTP_Termbox_InScissor(int x, int y) {
    if (!progtp_tb_scissor_enabled) {
        return true;
    }
    return x >= progtp_tb_scissor.x &&
           y >= progtp_tb_scissor.y &&
           x < progtp_tb_scissor.x + progtp_tb_scissor.width &&
           y < progtp_tb_scissor.y + progtp_tb_scissor.height;
}

static bool ProgTP_Termbox_EnsureBackgrounds(void) {
    int width = tb_width();
    int height = tb_height();
    if (width <= 0 || height <= 0) {
        return false;
    }

    size_t count = (size_t)width * (size_t)height;
    if (width != progtp_tb_background_width || height != progtp_tb_background_height) {
        uintattr_t *next = realloc(progtp_tb_backgrounds, count * sizeof(*next));
        if (!next) {
            return false;
        }
        progtp_tb_backgrounds = next;
        progtp_tb_background_width = width;
        progtp_tb_background_height = height;
    }

    for (size_t i = 0; i < count; ++i) {
        progtp_tb_backgrounds[i] = TB_DEFAULT;
    }
    return true;
}

static uintattr_t ProgTP_Termbox_BackgroundAt(int x, int y) {
    if (!progtp_tb_backgrounds ||
        x < 0 ||
        y < 0 ||
        x >= progtp_tb_background_width ||
        y >= progtp_tb_background_height) {
        return TB_DEFAULT;
    }
    return progtp_tb_backgrounds[(size_t)y * (size_t)progtp_tb_background_width + (size_t)x];
}

static void ProgTP_Termbox_SetCell(int x, int y, uint32_t ch, uintattr_t fg, uintattr_t bg) {
    if (x < 0 || y < 0 || x >= tb_width() || y >= tb_height() || !ProgTP_Termbox_InScissor(x, y)) {
        return;
    }

    uintattr_t resolved_bg = bg == PROGTP_TB_KEEP_BACKGROUND ? ProgTP_Termbox_BackgroundAt(x, y) : bg;
    if (bg != PROGTP_TB_KEEP_BACKGROUND && progtp_tb_backgrounds) {
        progtp_tb_backgrounds[(size_t)y * (size_t)progtp_tb_background_width + (size_t)x] = bg;
    }
    tb_set_cell(x, y, ch, fg, resolved_bg);
}

static void ProgTP_Termbox_DrawRectangle(ProgTP_TermboxBox box, Clay_Color color) {
    uintattr_t background = ProgTP_Termbox_Color(color);
    for (int y = box.y; y < box.y + box.height; ++y) {
        for (int x = box.x; x < box.x + box.width; ++x) {
            ProgTP_Termbox_SetCell(x, y, ' ', TB_DEFAULT, background);
        }
    }
}

static void ProgTP_Termbox_DrawBorder(ProgTP_TermboxBox box, Clay_BorderRenderData *border) {
    if (box.width <= 0 || box.height <= 0) {
        return;
    }

    uintattr_t color = ProgTP_Termbox_Color(border->color);
    int left = box.x;
    int right = box.x + box.width - 1;
    int top = box.y;
    int bottom = box.y + box.height - 1;

    if (border->width.top > 0) {
        for (int x = left; x <= right; ++x) {
            ProgTP_Termbox_SetCell(x, top, '-', color, PROGTP_TB_KEEP_BACKGROUND);
        }
    }
    if (border->width.bottom > 0) {
        for (int x = left; x <= right; ++x) {
            ProgTP_Termbox_SetCell(x, bottom, '-', color, PROGTP_TB_KEEP_BACKGROUND);
        }
    }
    if (border->width.left > 0) {
        for (int y = top; y <= bottom; ++y) {
            ProgTP_Termbox_SetCell(left, y, '|', color, PROGTP_TB_KEEP_BACKGROUND);
        }
    }
    if (border->width.right > 0) {
        for (int y = top; y <= bottom; ++y) {
            ProgTP_Termbox_SetCell(right, y, '|', color, PROGTP_TB_KEEP_BACKGROUND);
        }
    }

    ProgTP_Termbox_SetCell(left, top, '+', color, PROGTP_TB_KEEP_BACKGROUND);
    ProgTP_Termbox_SetCell(right, top, '+', color, PROGTP_TB_KEEP_BACKGROUND);
    ProgTP_Termbox_SetCell(left, bottom, '+', color, PROGTP_TB_KEEP_BACKGROUND);
    ProgTP_Termbox_SetCell(right, bottom, '+', color, PROGTP_TB_KEEP_BACKGROUND);
}

static void ProgTP_Termbox_DrawText(Clay_RenderCommand *command) {
    Clay_TextRenderData *text = &command->renderData.text;
    int x = ProgTP_Termbox_FloorCell(command->boundingBox.x, PROGTP_TB_CELL_WIDTH);
    int y = ProgTP_Termbox_FloorCell(command->boundingBox.y, PROGTP_TB_CELL_HEIGHT);
    uintattr_t color = ProgTP_Termbox_Color(text->textColor);

    for (int32_t i = 0; i < text->stringContents.length; ++i) {
        unsigned char ch = (unsigned char)text->stringContents.chars[i];
        if (ch >= 32 && ch < 127) {
            ProgTP_Termbox_SetCell(x + i, y, ch, color, PROGTP_TB_KEEP_BACKGROUND);
        }
    }
}

static void ProgTP_Termbox_Render(Clay_RenderCommandArray commands) {
    if (!ProgTP_Termbox_EnsureBackgrounds()) {
        return;
    }
    progtp_tb_scissor_enabled = false;

    for (int32_t i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
        ProgTP_TermboxBox box = ProgTP_Termbox_BoxFromClay(command->boundingBox);

        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                ProgTP_Termbox_DrawRectangle(box, command->renderData.rectangle.backgroundColor);
                break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                ProgTP_Termbox_DrawBorder(box, &command->renderData.border);
                break;
            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                ProgTP_Termbox_DrawText(command);
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                progtp_tb_scissor_enabled = true;
                progtp_tb_scissor = box;
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                progtp_tb_scissor_enabled = false;
                break;
            case CLAY_RENDER_COMMAND_TYPE_NONE:
            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                break;
        }
    }
}

#endif
