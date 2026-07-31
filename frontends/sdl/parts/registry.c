#include "parts/registry.h"

#include "parts/button.h"
#include "parts/buzzer.h"
#include "parts/led.h"
#include "parts/max7219.h"
#include "parts/led_matrix_8x8.h"
#include "parts/pic10.h"
#include "picemu/core/pic_device.h"

#include <stdio.h>
#include <string.h>

typedef bool (*SdlPartInitFn)(SdlPart *part,
                              const CircuitPartConfig *config);

typedef struct {
    const char *type;
    SdlPartInitFn init;
} SdlPartFactory;

/*
 * C 中使用显式注册表比编译器构造器更容易移植和理解。
 * 新增内建 SDL 器件时只需实现自己的模块，并在这里增加一项。
 */
static const SdlPartFactory FACTORIES[] = {
    {"led",        sdl_part_led_init},
    {"pushbutton", sdl_part_button_init},
    {"buzzer",     sdl_part_buzzer_init},
    {"max7219",    sdl_part_max7219_init},
    {"led-matrix-8x8", sdl_part_led_matrix_8x8_init}
};

bool sdl_part_create(SdlPart *part, const CircuitPartConfig *config,
                     char *error, size_t error_size)
{
    const PicDeviceDescription *pic_device;
    unsigned i;

    memset(part, 0, sizeof(*part));
    snprintf(part->id, sizeof(part->id), "%s", config->id);
    part->x = config->left;
    part->y = config->top;

    pic_device = pic_device_find(config->type);
    if (pic_device != NULL) {
        if (sdl_part_pic10_init(part, pic_device)) return true;
    } else {
        for (i = 0; i < sizeof(FACTORIES) / sizeof(FACTORIES[0]); ++i) {
            if (strcmp(config->type, FACTORIES[i].type) == 0) {
                if (FACTORIES[i].init(part, config)) return true;
                snprintf(error, error_size,
                         "无法创建器件：%s", config->id);
                return false;
            }
        }
    }

    snprintf(error, error_size, "不支持的器件类型：%s", config->type);
    return false;
}
