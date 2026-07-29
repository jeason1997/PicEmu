#include "sdl_parts.h"
#include "sdl_text.h"

#include <string.h>

void sdl_part_button_render(SDL_Renderer *renderer, const SdlPart *part)
{
    const SimButton *button = &part->device.button;
    SDL_Rect body = {part->x - 55, part->y - 30, 110, 60};
    SDL_Color text = {220, 225, 230, 255};
    SDL_SetRenderDrawColor(renderer,
                          button->pressed ? 75 : 155,
                          button->pressed ? 80 : 160,
                          button->pressed ? 85 : 170, 255);
    SDL_RenderFillRect(renderer, &body);
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderDrawRect(renderer, &body);
    sdl_draw_text(renderer, part->x - sdl_text_width(part->id, 1) / 2,
                  part->y + 40, 1, part->id, text);
}

bool sdl_part_button_pin(const SdlPart *part, const char *name,
                         unsigned *device_pin, SDL_Point *point)
{
    if (strcmp(name, "1") != 0 && strcmp(name, "OUT") != 0) return false;
    *device_pin = 0;
    point->x = part->x - 55;
    point->y = part->y;
    return true;
}

bool sdl_part_button_hit(const SdlPart *part, int x, int y)
{
    return x >= part->x - 55 && x < part->x + 55 &&
           y >= part->y - 30 && y < part->y + 30;
}
