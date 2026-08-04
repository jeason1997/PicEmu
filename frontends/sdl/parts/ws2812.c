#include "parts/ws2812.h"
#include "picemu/sim/devices/ws2812.h"

#include <stdlib.h>
#include <string.h>

static void render(SDL_Renderer *renderer, const SdlPart *part)
{
    const SimWs2812 *strip = part->device->state;
    unsigned row, col;
    SDL_Rect body = {part->x, part->y,
                     (int)strip->cols * 40 + 20,
                     (int)strip->rows * 40 + 20};
    SDL_SetRenderDrawColor(renderer, 34, 39, 45, 255);
    SDL_RenderFillRect(renderer, &body);
    for (row = 0; row < strip->rows; ++row) for (col = 0; col < strip->cols; ++col) {
        unsigned physical = row * strip->cols +
            (strip->serpentine && (row & 1u) ? strip->cols - 1u - col : col);
        SDL_Rect led = {part->x + 14 + (int)col * 40,
                        part->y + 14 + (int)row * 40, 28, 28};
        uint8_t r = strip->colors[physical][0], g = strip->colors[physical][1];
        uint8_t b = strip->colors[physical][2];
        SDL_SetRenderDrawColor(renderer, r ? r : 24, g ? g : 24,
                              b ? b : 24, 255);
        SDL_RenderFillRect(renderer, &led);
    }
}

static bool find_pin(const SdlPart *part, const char *name, unsigned *pin,
                     SDL_Point *point, bool *is_signal)
{
    if (strcmp(name, "DIN") != 0) return false;
    *pin = 0; *is_signal = true;
    point->x = part->x; point->y = part->y + 20;
    return true;
}

static void destroy(SdlPart *part)
{
    free(part->view_state); part->view_state = NULL; part->device = NULL;
}

static const SdlPartOps OPS = {.render = render, .find_pin = find_pin,
                               .destroy = destroy};

bool sdl_part_ws2812_init(SdlPart *part,
                          const CircuitPartConfig *config)
{
    SimWs2812 *strip = malloc(sizeof(*strip));
    unsigned rows = (unsigned)strtoul(
        circuit_part_get(config, "rows", "2"), NULL, 10);
    unsigned cols = (unsigned)strtoul(
        circuit_part_get(config, "cols", "2"), NULL, 10);
    const char *wiring = circuit_part_get(config, "wiring", "Z");
    if (strip == NULL) return false;
    if (rows == 0 || rows > 32 || cols == 0 || cols > 32) {
        free(strip);
        return false;
    }
    sim_ws2812_init(strip, part->id, rows * cols);
    strip->rows = rows;
    strip->cols = cols;
    strip->serpentine = strcmp(wiring, "S") == 0 || strcmp(wiring, "s") == 0;
    part->device = &strip->base; part->view_state = strip; part->ops = &OPS;
    return true;
}
