#include "parts/sdl_parts.h"
#include "common/sdl_text.h"

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

void sdl_part_led_render(SDL_Renderer *renderer, const SdlPart *part)
{
    const SimLed *led = &part->device.led;
    SDL_Color text = {210, 215, 220, 255};
    SDL_SetRenderDrawColor(renderer,
                          led->lit ? led->red : led->red / 5,
                          led->lit ? led->green : led->green / 5,
                          led->lit ? led->blue : led->blue / 5, 255);
    filled_circle(renderer, part->x, part->y, 24);
    sdl_draw_text(renderer, part->x - 18, part->y + 34, 1,
                  part->id, text);
}

bool sdl_part_led_pin(const SdlPart *part, const char *name,
                      unsigned *device_pin, SDL_Point *point)
{
    if (strcmp(name, "A") != 0 && strcmp(name, "IN") != 0) return false;
    *device_pin = 0;
    point->x = part->x + 24;
    point->y = part->y;
    return true;
}
