#ifndef PROGTP_CLAY_RENDERER_SDL3_BASIC_H
#define PROGTP_CLAY_RENDERER_SDL3_BASIC_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <clay.h>

typedef struct {
    SDL_Renderer *renderer;
    const char *fontPath;
    TTF_Font *fontCache[128];
} Clay_SDL3RendererData;

static uint32_t ProgTP_SDL3_ClampFontSize(uint32_t fontSize) {
    if (fontSize == 0) {
        return 1;
    }
    if (fontSize >= 128) {
        return 127;
    }
    return fontSize;
}

static TTF_Font *ProgTP_SDL3_GetFont(Clay_SDL3RendererData *rendererData, uint32_t fontSize) {
    uint32_t cacheIndex = ProgTP_SDL3_ClampFontSize(fontSize);
    TTF_Font *font = rendererData->fontCache[cacheIndex];
    if (!font && rendererData->fontPath) {
        font = TTF_OpenFont(rendererData->fontPath, (float)cacheIndex);
        if (font) {
            TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
            rendererData->fontCache[cacheIndex] = font;
        }
    }
    return font;
}

static void ProgTP_SDL3_CloseFonts(Clay_SDL3RendererData *rendererData) {
    for (size_t i = 0; i < SDL_arraysize(rendererData->fontCache); ++i) {
        if (rendererData->fontCache[i]) {
            TTF_CloseFont(rendererData->fontCache[i]);
            rendererData->fontCache[i] = NULL;
        }
    }
}

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
                TTF_Font *font = ProgTP_SDL3_GetFont(rendererData, config->fontSize);
                if (!font) {
                    break;
                }

                SDL_Surface *surface = TTF_RenderText_Blended(
                    font,
                    config->stringContents.chars,
                    (size_t)config->stringContents.length,
                    (SDL_Color){
                        (Uint8)config->textColor.r,
                        (Uint8)config->textColor.g,
                        (Uint8)config->textColor.b,
                        (Uint8)config->textColor.a,
                    });
                if (surface) {
                    SDL_Texture *texture = SDL_CreateTextureFromSurface(rendererData->renderer, surface);
                    if (texture) {
                        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
                        SDL_FRect destination = {
                            .x = rect.x,
                            .y = rect.y,
                            .w = (float)surface->w,
                            .h = (float)surface->h,
                        };
                        SDL_RenderTexture(rendererData->renderer, texture, NULL, &destination);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
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
