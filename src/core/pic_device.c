#include "picemu/core/pic_device.h"

#include <ctype.h>
#include <string.h>

const PicDeviceDescription PIC_DEVICE_PIC10F200 = {
    .name = "PIC10F200",
    .core_family = PIC_CORE_BASELINE_12,
    .instruction_width = 12,
    .program_words = 256,
    .ram_bytes = 16,
    .stack_depth = 2,
    .gpio_count = 4,
    .pins = {
        {"GP0/ICSPDAT", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_OUTPUT |
                         PIC_PIN_CAP_ICSP},
        {"GP1/ICSPCLK", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_OUTPUT |
                         PIC_PIN_CAP_ICSP},
        {"GP2/T0CKI/FOSC4", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_OUTPUT |
                             PIC_PIN_CAP_T0CKI},
        {"GP3/MCLR/VPP", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_MCLR |
                          PIC_PIN_CAP_ICSP}
    }
};

const PicDeviceDescription PIC_DEVICE_PIC10F202 = {
    .name = "PIC10F202",
    .core_family = PIC_CORE_BASELINE_12,
    .instruction_width = 12,
    .program_words = 512,
    .ram_bytes = 24,
    .stack_depth = 2,
    .gpio_count = 4,
    .pins = {
        {"GP0/ICSPDAT", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_OUTPUT |
                         PIC_PIN_CAP_ICSP},
        {"GP1/ICSPCLK", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_OUTPUT |
                         PIC_PIN_CAP_ICSP},
        {"GP2/T0CKI/FOSC4", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_OUTPUT |
                             PIC_PIN_CAP_T0CKI},
        {"GP3/MCLR/VPP", PIC_PIN_CAP_INPUT | PIC_PIN_CAP_MCLR |
                          PIC_PIN_CAP_ICSP}
    }
};

static int equal_ignore_case(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) !=
            tolower((unsigned char)*right)) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

const PicDeviceDescription *pic_device_find(const char *name)
{
    if (name != NULL &&
        (equal_ignore_case(name, "PIC10F200") ||
         equal_ignore_case(name, "10F200"))) {
        return &PIC_DEVICE_PIC10F200;
    }
    if (name != NULL &&
        (equal_ignore_case(name, "PIC10F202") ||
         equal_ignore_case(name, "10F202"))) {
        return &PIC_DEVICE_PIC10F202;
    }
    return NULL;
}
