#ifndef STM32_PIC_FIRMWARE_H
#define STM32_PIC_FIRMWARE_H

#include "picemu/firmware/hex_loader.h"

/*
 * 该符号由Makefile调用hex2c，根据EXAMPLE选择自动生成。
 * 平台代码不需要知道固件来自blink、button还是其他示例。
 */
extern const HexImage pic_firmware_image;

#endif
