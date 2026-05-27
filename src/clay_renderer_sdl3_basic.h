#ifndef PROGTP_CLAY_RENDERER_SDL3_BASIC_H
#define PROGTP_CLAY_RENDERER_SDL3_BASIC_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <clay.h>

typedef struct {
    SDL_Renderer *renderer;
    TTF_TextEngine *textEngine;
    TTF_Font **fonts;
} Clay_SDL3RendererData;

static SDL_FRect ProgTP_SDL3_RectFromClay(Clay_BoundingBox box) {
    return (SDL_FRect){
        .x = box.x,
        .y = box.y,
        .w = box.width,
        .h = box.height,
    };
}

static void ProgTP_SDL3_SetColor(SDL_Renderer *renderer, Clay_Color color) {
    SDL_SetRenderDrawColor(
        renderer,
        (Uint8)color.r,
        (Uint8)color.g,
        (Uint8)color.b,
        (Uint8)color.a);
}

static void ProgTP_SDL3_FillRect(SDL_Renderer *renderer, SDL_FRect rect) {
    if (rect.w > 0.0f && rect.h > 0.0f) {
        SDL_RenderFillRect(renderer, &rect);
    }
}

static void ProgTP_SDL3_RenderBorder(SDL_Renderer *renderer, SDL_FRect rect, Clay_BorderRenderData *border) {
    ProgTP_SDL3_SetColor(renderer, border->color);

    ProgTP_SDL3_FillRect(renderer, (SDL_FRect){
        .x = rect.x,
        .y = rect.y,
        .w = rect.w,
        .h = (float)border->width.top,
    });
    ProgTP_SDL3_FillRect(renderer, (SDL_FRect){
        .x = rect.x,
        .y = rect.y + rect.h - (float)border->width.bottom,
        .w = rect.w,
        .h = (float)border->width.bottom,
    });
    ProgTP_SDL3_FillRect(renderer, (SDL_FRect){
        .x = rect.x,
        .y = rect.y,
        .w = (float)border->width.left,
        .h = rect.h,
    });
    ProgTP_SDL3_FillRect(renderer, (SDL_FRect){
        .x = rect.x + rect.w - (float)border->width.right,
        .y = rect.y,
        .w = (float)border->width.right,
        .h = rect.h,
    });
}

static void SDL_Clay_RenderClayCommands(Clay_SDL3RendererData *rendererData, Clay_RenderCommandArray *commands) {
    SDL_SetRenderDrawBlendMode(rendererData->renderer, SDL_BLENDMODE_BLEND);

    for (int32_t i = 0; i < commands->length; ++i) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(commands, i);
        SDL_FRect rect = ProgTP_SDL3_RectFromClay(command->boundingBox);

        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData *config = &command->renderData.rectangle;
                ProgTP_SDL3_SetColor(rendererData->renderer, config->backgroundColor);
                ProgTP_SDL3_FillRect(rendererData->renderer, rect);
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                ProgTP_SDL3_RenderBorder(rendererData->renderer, rect, &command->renderData.border);
                break;

            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_TextRenderData *config = &command->renderData.text;
                TTF_Font *font = rendererData->fonts[config->fontId];
                if (!font || !rendererData->textEngine) {
                    break;
                }

                TTF_SetFontSize(font, config->fontSize);
                TTF_Text *text = TTF_CreateText(
                    rendererData->textEngine,
                    font,
                    config->stringContents.chars,
                    (size_t)config->stringContents.length);
                if (text) {
                    TTF_SetTextColor(
                        text,
                        (Uint8)config->textColor.r,
                        (Uint8)config->textColor.g,
                        (Uint8)config->textColor.b,
                        (Uint8)config->textColor.a);
                    TTF_DrawRendererText(text, rect.x, rect.y);
                    TTF_DestroyText(text);
                }
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                SDL_Rect clip = {
                    .x = (int)rect.x,
                    .y = (int)rect.y,
                    .w = (int)rect.w,
                    .h = (int)rect.h,
                };
                SDL_SetRenderClipRect(rendererData->renderer, &clip);
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                SDL_SetRenderClipRect(rendererData->renderer, NULL);
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
