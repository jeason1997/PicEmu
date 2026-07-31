#include "parts/max7219.h"
#include "common/sdl_text.h"
#include "picemu/sim/devices/max7219.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void render(SDL_Renderer *renderer, const SdlPart *part)
{
    SDL_Rect body = {part->x, part->y, 190, 235};
    SDL_Color text = {225, 232, 240, 255};
    SDL_SetRenderDrawColor(renderer, 32, 42, 53, 255);
    SDL_RenderFillRect(renderer, &body);
    SDL_SetRenderDrawColor(renderer, 133, 148, 165, 255);
    SDL_RenderDrawRect(renderer, &body);
    sdl_draw_text(renderer, part->x + 55, part->y + 105, 2,
                  "MAX7219", text);
}

static bool find_pin(const SdlPart *part, const char *name, unsigned *pin,
                     SDL_Point *point, bool *is_signal)
{
    unsigned index;
    if (strcmp(name, "DIN") == 0) { *pin = SIM_MAX7219_DIN; point->y = part->y + 25; }
    else if (strcmp(name, "CLK") == 0) { *pin = SIM_MAX7219_CLK; point->y = part->y + 75; }
    else if (strcmp(name, "LOAD") == 0) { *pin = SIM_MAX7219_LOAD; point->y = part->y + 125; }
    else if (sscanf(name, "SEG%u", &index) == 1 && index < 8) {
        *pin = SIM_MAX7219_SEG0 + index; point->y = part->y + 8 + (int)index * 13;
    } else if (sscanf(name, "DIG%u", &index) == 1 && index < 8) {
        *pin = SIM_MAX7219_DIG0 + index; point->y = part->y + 112 + (int)index * 13;
    } else return false;
    point->x = *pin < SIM_MAX7219_SEG0 ? part->x : part->x + 190;
    *is_signal = true;
    return true;
}

static void destroy(SdlPart *part)
{
    free(part->view_state); part->view_state = NULL; part->device = NULL;
}

static const SdlPartOps OPS = {.render = render, .find_pin = find_pin,
                               .destroy = destroy};

bool sdl_part_max7219_init(SdlPart *part, const CircuitPartConfig *config)
{
    SimMax7219 *chip = malloc(sizeof(*chip));
    (void)config;
    if (chip == NULL) return false;
    sim_max7219_init(chip, part->id);
    part->device = &chip->base; part->view_state = chip; part->ops = &OPS;
    return true;
}
