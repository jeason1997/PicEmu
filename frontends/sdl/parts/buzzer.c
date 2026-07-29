#include "parts/buzzer.h"
#include "common/sdl_text.h"
#include "picemu/sim/devices/buzzer.h"

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

static void buzzer_render(SDL_Renderer *renderer, const SdlPart *part)
{
    const SimBuzzer *buzzer = part->device->state;
    SDL_Color text = {220, 225, 230, 255};
    SDL_SetRenderDrawColor(renderer,
                          buzzer->active ? 245 : 80,
                          buzzer->active ? 190 : 80, 40, 255);
    filled_circle(renderer, part->x, part->y, 30);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    filled_circle(renderer, part->x, part->y, 10);
    sdl_draw_text(renderer, part->x - sdl_text_width(part->id, 1) / 2,
                  part->y + 40, 1, part->id, text);
}

static bool buzzer_find_pin(const SdlPart *part, const char *name,
                            unsigned *device_pin, SDL_Point *point,
                            bool *is_signal)
{
    if (strcmp(name, "1") != 0 && strcmp(name, "IN") != 0) return false;
    *device_pin = 0;
    *is_signal = true;
    point->x = part->x - 30;
    point->y = part->y;
    return true;
}

static void buzzer_destroy(SdlPart *part)
{
    free(part->view_state);
    part->view_state = NULL;
    part->device = NULL;
}

static double buzzer_audio_frequency(const SdlPart *part)
{
    const SimBuzzer *buzzer = part->device->state;
    if (buzzer->frequency_hz > 0.0) return buzzer->frequency_hz;
    /* 兼容引脚持续为高电平的传统有源蜂鸣器。 */
    return buzzer->active ? 2000.0 : 0.0;
}

static const SdlPartOps BUZZER_OPS = {
    .render = buzzer_render,
    .find_pin = buzzer_find_pin,
    .audio_frequency = buzzer_audio_frequency,
    .destroy = buzzer_destroy
};

bool sdl_part_buzzer_init(SdlPart *part, const CircuitPartConfig *config)
{
    SimBuzzer *buzzer = malloc(sizeof(*buzzer));
    (void)config;
    if (buzzer == NULL) return false;

    sim_buzzer_init(buzzer, part->id);
    part->device = &buzzer->base;
    part->view_state = buzzer;
    part->ops = &BUZZER_OPS;
    return true;
}
