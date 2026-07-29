#ifndef SDL_PARTS_H
#define SDL_PARTS_H

#include "picemu/sim/circuit_config.h"
#include "picemu/sim/device.h"

#include <SDL2/SDL.h>

#include <stdbool.h>

typedef enum {
    SDL_PART_PIC10F200,
    SDL_PART_LED,
    SDL_PART_BUTTON,
    SDL_PART_BUZZER
} SdlPartType;

typedef struct {
    char id[CIRCUIT_TEXT_LENGTH];
    SdlPartType type;
    int x;
    int y;
    union {
        SimLed led;
        SimButton button;
        SimBuzzer buzzer;
    } device;
} SdlPart;

void sdl_part_pic10f200_render(SDL_Renderer *renderer,
                              const SdlPart *part);
bool sdl_part_pic10f200_pin(const SdlPart *part, const char *name,
                           unsigned *gpio, SDL_Point *point,
                           bool *is_signal);

void sdl_part_led_render(SDL_Renderer *renderer, const SdlPart *part);
bool sdl_part_led_pin(const SdlPart *part, const char *name,
                      unsigned *device_pin, SDL_Point *point);

void sdl_part_button_render(SDL_Renderer *renderer, const SdlPart *part);
bool sdl_part_button_pin(const SdlPart *part, const char *name,
                         unsigned *device_pin, SDL_Point *point);
bool sdl_part_button_hit(const SdlPart *part, int x, int y);

void sdl_part_buzzer_render(SDL_Renderer *renderer, const SdlPart *part);
bool sdl_part_buzzer_pin(const SdlPart *part, const char *name,
                         unsigned *device_pin, SDL_Point *point);

#endif
