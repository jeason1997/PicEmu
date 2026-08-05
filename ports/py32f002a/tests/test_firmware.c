#include "firmware.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    HexImage loaded;
    char error[256];

    if (argc != 2) {
        fprintf(stderr, "用法：%s firmware.hex\n", argv[0]);
        return 1;
    }
    if (!hex_load_file(argv[1], &loaded, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    if (memcmp(pic_firmware_image.program, loaded.program,
               sizeof(loaded.program)) != 0 ||
        pic_firmware_image.config_word != loaded.config_word ||
        pic_firmware_image.config_present != loaded.config_present) {
        fprintf(stderr, "自动生成的 PY32 内嵌固件与 HEX 不一致\n");
        return 1;
    }
    puts("PY32F002A 内嵌固件校验通过。");
    return 0;
}
