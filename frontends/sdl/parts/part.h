#ifndef SDL_PART_H
#define SDL_PART_H

#include "picemu/sim/circuit_config.h"
#include "picemu/sim/device.h"

#include <SDL2/SDL.h>

#include <stdbool.h>

typedef struct SdlPart SdlPart;

/*
 * SDL 器件的通用视图接口。电路层只通过这组操作访问器件，
 * 不需要知道 LED、按键或蜂鸣器的具体结构。
 */
typedef struct {
    void (*render)(SDL_Renderer *renderer, const SdlPart *part);
    bool (*find_pin)(const SdlPart *part, const char *name,
                     unsigned *pin, SDL_Point *point, bool *is_signal);
    void (*mouse)(SdlPart *part, int x, int y, bool pressed);
    double (*audio_frequency)(const SdlPart *part);
    void (*destroy)(SdlPart *part);
} SdlPartOps;

struct SdlPart {
    char id[CIRCUIT_TEXT_LENGTH];
    int x;
    int y;
    bool is_mcu;
    SimDevice *device;
    void *view_state;
    const SdlPartOps *ops;
};

void sdl_part_render(SDL_Renderer *renderer, const SdlPart *part);
bool sdl_part_find_pin(const SdlPart *part, const char *name,
                       unsigned *pin, SDL_Point *point, bool *is_signal);
void sdl_part_mouse(SdlPart *part, int x, int y, bool pressed);
double sdl_part_audio_frequency(const SdlPart *part);
void sdl_part_destroy(SdlPart *part);

#endif
