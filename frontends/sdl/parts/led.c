#include "parts/led.h"
#include "common/sdl_text.h"
#include "picemu/sim/devices/led.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void filled_circle(SDL_Renderer *renderer, int x, int y, int radius)
{
    int row;
    for (row = -radius; row <= radius; ++row) {
        int width = (int)sqrt((double)(radius * radius - row * row));
        SDL_RenderDrawLine(renderer, x - width, y + row, x + width, y + row);
    }
}

static void led_render(SDL_Renderer *renderer, const SdlPart *part)
{
    const SimLed *led = part->device->state;
    SDL_Color text = {210, 215, 220, 255};
    unsigned intensity = 26u + (unsigned)led->brightness * 229u / 255u;
    SDL_SetRenderDrawColor(renderer,
                          (uint8_t)((unsigned)led->red * intensity / 255u),
                          (uint8_t)((unsigned)led->green * intensity / 255u),
                          (uint8_t)((unsigned)led->blue * intensity / 255u),
                          255);
    filled_circle(renderer, part->x, part->y, 24);
    sdl_draw_text(renderer, part->x - 18, part->y + 34, 1,
                  part->id, text);
}

static bool led_find_pin(const SdlPart *part, const char *name,
                         unsigned *device_pin, SDL_Point *point,
                         bool *is_signal)
{
    if (strcmp(name, "A") != 0 && strcmp(name, "IN") != 0) return false;
    *device_pin = 0;
    *is_signal = true;
    point->x = part->x + 24;
    point->y = part->y;
    return true;
}

static void led_destroy(SdlPart *part)
{
    free(part->view_state);
    part->view_state = NULL;
    part->device = NULL;
}

static const SdlPartOps LED_OPS = {
    .render = led_render,
    .find_pin = led_find_pin,
    .destroy = led_destroy
};

bool sdl_part_led_init(SdlPart *part, const CircuitPartConfig *config)
{
    SimLed *led = malloc(sizeof(*led));
    const char *color = circuit_part_get(config, "color", "red");
    uint8_t r = 255, g = 45, b = 35;

    if (led == NULL) return false;
    if (strcmp(color, "green") == 0) {
        r = 45; g = 255; b = 65;
    } else if (strcmp(color, "blue") == 0) {
        r = 45; g = 110; b = 255;
    } else if (strcmp(color, "yellow") == 0) {
        r = 255; g = 210; b = 35;
    }

    sim_led_init(led, part->id, r, g, b, true);
    part->device = &led->base;
    part->view_state = led;
    part->ops = &LED_OPS;
    return true;
}
