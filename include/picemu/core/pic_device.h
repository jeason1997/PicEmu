#ifndef PIC_DEVICE_H
#define PIC_DEVICE_H

#include <stdint.h>

#define PIC_DEVICE_MAX_PINS 8u

typedef enum {
    PIC_CORE_BASELINE_12,
    PIC_CORE_MIDRANGE_14,
    PIC_CORE_ENHANCED_MIDRANGE_14
} PicCoreFamily;

enum {
    PIC_PIN_CAP_INPUT  = 1u << 0,
    PIC_PIN_CAP_OUTPUT = 1u << 1,
    PIC_PIN_CAP_MCLR   = 1u << 2,
    PIC_PIN_CAP_T0CKI  = 1u << 3,
    PIC_PIN_CAP_ICSP   = 1u << 4
};

typedef struct {
    const char *name;
    uint8_t capabilities;
} PicPinDescription;

/*
 * 描述一个PIC型号的静态硬件能力。桌面前端、VCD和硬件桥接层只依赖
 * 这个描述，而不把GPIO数量、程序容量等信息写死在界面代码中。
 */
typedef struct {
    const char *name;
    PicCoreFamily core_family;
    unsigned instruction_width;
    unsigned program_words;
    unsigned ram_bytes;
    unsigned stack_depth;
    unsigned gpio_count;
    PicPinDescription pins[PIC_DEVICE_MAX_PINS];
} PicDeviceDescription;

extern const PicDeviceDescription PIC_DEVICE_PIC10F200;

/* 后续加入PIC10F202/204/206时，只需注册新的描述和对应核心实现。 */
const PicDeviceDescription *pic_device_find(const char *name);

#endif
