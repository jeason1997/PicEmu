#ifndef SDL_CIRCUIT_H
#define SDL_CIRCUIT_H

#include "picemu/sim/circuit_config.h"
#include "picemu/firmware/hex_loader.h"
#include "picemu/core/pic10_cpu.h"
#include "parts/sdl_parts.h"
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
    Pic10Cpu cpu;
    SimBoard board;
    SdlPart parts[CIRCUIT_MAX_PARTS];
    unsigned part_count;
    SdlWire wires[CIRCUIT_MAX_CONNECTIONS];
    unsigned wire_count;
} SdlCircuit;

bool sdl_circuit_init(SdlCircuit *circuit, const CircuitConfig *config,
                      const HexImage *image, char *error,
                      size_t error_size);
void sdl_circuit_reset(SdlCircuit *circuit);
void sdl_circuit_step(SdlCircuit *circuit);
void sdl_circuit_render(SDL_Renderer *renderer,
                        const SdlCircuit *circuit, bool running);
void sdl_circuit_mouse(SdlCircuit *circuit, int x, int y, bool pressed);
bool sdl_circuit_buzzer_active(const SdlCircuit *circuit);
double sdl_circuit_buzzer_frequency(const SdlCircuit *circuit);

#endif
