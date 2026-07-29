#ifndef SDL_TEXT_H
#define SDL_TEXT_H

#include <SDL2/SDL.h>

void sdl_draw_text(SDL_Renderer *renderer, int x, int y, int scale,
                   const char *text, SDL_Color color);
int sdl_text_width(const char *text, int scale);

#endif
