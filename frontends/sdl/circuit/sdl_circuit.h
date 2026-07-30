#ifndef SDL_CIRCUIT_H
#define SDL_CIRCUIT_H

#include "picemu/sim/circuit_config.h"
#include "picemu/firmware/hex_loader.h"
#include "picemu/core/pic10_cpu.h"
#include "picemu/sim/mcu/pic10.h"
#include "parts/part.h"
#include "picemu/sim/board.h"

#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned part_a;
    unsigned part_b;
    char pin_a[32];
    char pin_b[32];
    SDL_Color color;
    int net;
} SdlWire;

typedef struct {
    Pic10Cpu cpus[CIRCUIT_MAX_PARTS];
    SimPic10Mcu mcu_adapters[CIRCUIT_MAX_PARTS];
    SimMcu *part_mcus[CIRCUIT_MAX_PARTS];
    unsigned mcu_count;
    SimBoard board;
    SdlPart parts[CIRCUIT_MAX_PARTS];
    unsigned part_count;
    SdlWire wires[CIRCUIT_MAX_CONNECTIONS];
    unsigned wire_count;
} SdlCircuit;

bool sdl_circuit_init(SdlCircuit *circuit, const CircuitConfig *config,
                      const HexImage *fallback_image, char *error,
                      size_t error_size);
bool sdl_circuit_init_with_override(
    SdlCircuit *circuit, const CircuitConfig *config,
    const HexImage *override_image, char *error, size_t error_size);
bool sdl_circuit_all_stopped(const SdlCircuit *circuit);
void sdl_circuit_reset(SdlCircuit *circuit);
void sdl_circuit_destroy(SdlCircuit *circuit);
/* 执行一条MCU指令，并返回该指令实际消耗的时钟周期数。 */
unsigned sdl_circuit_step(SdlCircuit *circuit);
void sdl_circuit_render(SDL_Renderer *renderer,
                        const SdlCircuit *circuit, bool running,
                        float zoom, int pan_x, int pan_y);
void sdl_circuit_mouse(SdlCircuit *circuit, int x, int y, bool pressed);
double sdl_circuit_buzzer_frequency(const SdlCircuit *circuit);

#endif
