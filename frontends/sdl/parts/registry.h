#ifndef SDL_PART_REGISTRY_H
#define SDL_PART_REGISTRY_H

#include "parts/part.h"

#include <stddef.h>

bool sdl_part_create(SdlPart *part, const CircuitPartConfig *config,
                     char *error, size_t error_size);

#endif
