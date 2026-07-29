#include "parts/sdl_parts.h"
#include "common/sdl_text.h"

#include <string.h>

enum { CHIP_WIDTH = 300, CHIP_HEIGHT = 240, PIN_LENGTH = 35 };

typedef struct {
    const char *name;
    const char *label;
    unsigned physical;
    unsigned gpio;
    bool signal;
    bool right;
    int offset_y;
} PinVisual;

static const PinVisual PINS[] = {
    {"GP0", "1 GP0/ICSPDAT", 1, 0, true,  false, 50},
    {"VSS", "2 VSS",         2, 0, false, false, 110},
    {"GP1", "3 GP1/ICSPCLK", 3, 1, true,  false, 170},
    {"GP3", "6 GP3/MCLR/VPP",6, 3, true,  true,  50},
    {"VDD", "5 VDD",         5, 0, false, true,  110},
    {"GP2", "4 GP2/T0CKI/FOSC4", 4, 2, true, true, 170}
};

void sdl_part_pic10f200_render(SDL_Renderer *renderer,
                              const SdlPart *part)
{
    SDL_Rect body = {part->x, part->y, CHIP_WIDTH, CHIP_HEIGHT};
    SDL_Rect pin_one_mark = {part->x + 12, part->y + 12, 10, 10};
    SDL_Color label = {225, 228, 232, 255};
    unsigned i;
    const char *title = "PIC10F200";

    SDL_SetRenderDrawColor(renderer, 42, 47, 54, 255);
    SDL_RenderFillRect(renderer, &body);
    SDL_SetRenderDrawColor(renderer, 190, 195, 200, 255);
    SDL_RenderDrawRect(renderer, &body);
    /* 封装上的圆点用于识别1脚方向。 */
    SDL_SetRenderDrawColor(renderer, 210, 210, 210, 255);
    SDL_RenderFillRect(renderer, &pin_one_mark);
    sdl_draw_text(renderer,
                  part->x + (CHIP_WIDTH - sdl_text_width(title, 3)) / 2,
                  part->y + 8, 3, title, label);

    for (i = 0; i < sizeof(PINS) / sizeof(PINS[0]); ++i) {
        const PinVisual *pin = &PINS[i];
        /*
         * 芯片内部划分为左右两列。PIC10F200最长的复用功能名称较长，
         * 使用1倍字体才能保证两列之间留有空隙；窗口本身仍支持整体缩放。
         */
        const int label_scale = 1;
        int y = part->y + pin->offset_y;
        int body_x = pin->right ? part->x + CHIP_WIDTH : part->x;
        int outside_x = pin->right ? body_x + PIN_LENGTH :
                                     body_x - PIN_LENGTH;
        int text_x = pin->right
            ? part->x + CHIP_WIDTH - 12 -
                sdl_text_width(pin->label, label_scale)
            : part->x + 12;

        SDL_SetRenderDrawColor(renderer, 195, 200, 205, 255);
        SDL_RenderDrawLine(renderer, body_x, y, outside_x, y);
        /*
         * 文字放在芯片轮廓内、引脚中心线上方，外部导线因此不会穿过
         * 字形，也不会和靠近芯片摆放的LED等器件重叠。
         */
        sdl_draw_text(renderer, text_x, y - 11, label_scale,
                      pin->label, label);
    }
}

bool sdl_part_pic10f200_pin(const SdlPart *part, const char *name,
                           unsigned *gpio, SDL_Point *point,
                           bool *is_signal)
{
    unsigned i;
    for (i = 0; i < sizeof(PINS) / sizeof(PINS[0]); ++i) {
        const PinVisual *pin = &PINS[i];
        if (strcmp(name, pin->name) == 0) {
            *gpio = pin->gpio;
            *is_signal = pin->signal;
            point->x = pin->right ? part->x + CHIP_WIDTH + PIN_LENGTH :
                                    part->x - PIN_LENGTH;
            point->y = part->y + pin->offset_y;
            return true;
        }
    }
    return false;
}
