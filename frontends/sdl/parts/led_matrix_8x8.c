#include "parts/led_matrix_8x8.h"
#include "picemu/sim/devices/led_matrix_8x8.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void render(SDL_Renderer *renderer, const SdlPart *part)
{
    const SimLedMatrix8x8 *matrix = part->device->state;
    SDL_Rect body = {part->x, part->y, 245, 245};
    unsigned row, column;
    SDL_SetRenderDrawColor(renderer, 25, 13, 16, 255);
    SDL_RenderFillRect(renderer, &body);
    for (row = 0; row < 8; ++row) for (column = 0; column < 8; ++column) {
        SDL_Rect led = {part->x + 42 + (int)column * 22,
                        part->y + 26 + (int)row * 22, 15, 15};
        bool on = (sim_led_matrix_8x8_row(matrix, row) & (1u << column)) != 0;
        SDL_SetRenderDrawColor(renderer, on ? 255 : 76,
                              on ? 53 : 23, on ? 69 : 29, 255);
        SDL_RenderFillRect(renderer, &led);
    }
}

static bool find_pin(const SdlPart *part, const char *name, unsigned *pin,
                     SDL_Point *point, bool *is_signal)
{
    unsigned index;
    if (sscanf(name, "SEG%u", &index) == 1 && index < 8) {
        *pin = index; point->x = part->x;
    } else if (sscanf(name, "DIG%u", &index) == 1 && index < 8) {
        *pin = 8 + index; point->x = part->x + 245;
    } else return false;
    point->y = part->y + 8 + (int)index * 27;
    *is_signal = true;
    return true;
}

static void destroy(SdlPart *part)
{
    free(part->view_state); part->view_state = NULL; part->device = NULL;
}

static const SdlPartOps OPS = {.render = render, .find_pin = find_pin,
                               .destroy = destroy};

bool sdl_part_led_matrix_8x8_init(SdlPart *part,
                                  const CircuitPartConfig *config)
{
    SimLedMatrix8x8 *matrix = malloc(sizeof(*matrix));
    bool common_cathode = circuit_part_get_bool(config, "commonCathode", true);
    if (matrix == NULL) return false;
    sim_led_matrix_8x8_init(matrix, part->id, common_cathode);
    part->device = &matrix->base; part->view_state = matrix; part->ops = &OPS;
    return true;
}
