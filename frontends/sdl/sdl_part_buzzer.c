#include "sdl_parts.h"
#include "sdl_text.h"

#include <math.h>
#include <string.h>

static void filled_circle(SDL_Renderer *renderer, int x, int y, int radius)
{
    int row;
    for (row = -radius; row <= radius; ++row) {
        int width = (int)sqrt((double)(radius * radius - row * row));
        SDL_RenderDrawLine(renderer, x - width, y + row, x + width, y + row);
    }
}

void sdl_part_buzzer_render(SDL_Renderer *renderer, const SdlPart *part)
{
    SDL_Color text = {220, 225, 230, 255};
    SDL_SetRenderDrawColor(renderer,
                          part->device.buzzer.active ? 245 : 80,
                          part->device.buzzer.active ? 190 : 80, 40, 255);
    filled_circle(renderer, part->x, part->y, 30);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    filled_circle(renderer, part->x, part->y, 10);
    sdl_draw_text(renderer, part->x - sdl_text_width(part->id, 1) / 2,
                  part->y + 40, 1, part->id, text);
}

bool sdl_part_buzzer_pin(const SdlPart *part, const char *name,
                         unsigned *device_pin, SDL_Point *point)
{
    if (strcmp(name, "1") != 0 && strcmp(name, "IN") != 0) return false;
    *device_pin = 0;
    point->x = part->x - 30;
    point->y = part->y;
    return true;
}
