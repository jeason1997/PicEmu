#include "parts/part.h"

void sdl_part_render(SDL_Renderer *renderer, const SdlPart *part)
{
    if (part != NULL && part->ops != NULL && part->ops->render != NULL) {
        part->ops->render(renderer, part);
    }
}

bool sdl_part_find_pin(const SdlPart *part, const char *name,
                       unsigned *pin, SDL_Point *point, bool *is_signal)
{
    return part != NULL && part->ops != NULL &&
           part->ops->find_pin != NULL &&
           part->ops->find_pin(part, name, pin, point, is_signal);
}

void sdl_part_mouse(SdlPart *part, int x, int y, bool pressed)
{
    if (part != NULL && part->ops != NULL && part->ops->mouse != NULL) {
        part->ops->mouse(part, x, y, pressed);
    }
}

double sdl_part_audio_frequency(const SdlPart *part)
{
    if (part != NULL && part->ops != NULL &&
        part->ops->audio_frequency != NULL) {
        return part->ops->audio_frequency(part);
    }
    return 0.0;
}

void sdl_part_destroy(SdlPart *part)
{
    if (part != NULL && part->ops != NULL && part->ops->destroy != NULL) {
        part->ops->destroy(part);
    }
}
