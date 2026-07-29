#ifndef SDL_PART_PIC10_H
#define SDL_PART_PIC10_H

#include "parts/part.h"
#include "picemu/core/pic_device.h"

bool sdl_part_pic10_init(SdlPart *part,
                         const PicDeviceDescription *device);

#endif
