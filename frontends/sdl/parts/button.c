#include "parts/button.h"
#include "common/sdl_text.h"
#include "picemu/sim/devices/button.h"

#include <stdlib.h>
#include <string.h>

static void button_render(SDL_Renderer *renderer, const SdlPart *part)
{
    const SimButton *button = part->device->state;
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

static bool button_find_pin(const SdlPart *part, const char *name,
                            unsigned *device_pin, SDL_Point *point,
                            bool *is_signal)
{
    if (strcmp(name, "1") != 0 && strcmp(name, "OUT") != 0) return false;
    *device_pin = 0;
    *is_signal = true;
    point->x = part->x - 55;
    point->y = part->y;
    return true;
}

static bool button_hit(const SdlPart *part, int x, int y)
{
    return x >= part->x - 55 && x < part->x + 55 &&
           y >= part->y - 30 && y < part->y + 30;
}

static void button_mouse(SdlPart *part, int x, int y, bool pressed)
{
    SimButton *button = part->device->state;
    if (!pressed || button_hit(part, x, y)) {
        sim_button_set_pressed(button, pressed);
    }
}

static void button_destroy(SdlPart *part)
{
    free(part->view_state);
    part->view_state = NULL;
    part->device = NULL;
}

static const SdlPartOps BUTTON_OPS = {
    .render = button_render,
    .find_pin = button_find_pin,
    .mouse = button_mouse,
    .destroy = button_destroy
};

bool sdl_part_button_init(SdlPart *part, const CircuitPartConfig *config)
{
    SimButton *button = malloc(sizeof(*button));
    if (button == NULL) return false;

    sim_button_init(button, part->id,
                    circuit_part_get_bool(config, "activeLow", true));
    part->device = &button->base;
    part->view_state = button;
    part->ops = &BUTTON_OPS;
    return true;
}
