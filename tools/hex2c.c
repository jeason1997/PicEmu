#include "picemu/firmware/hex_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static int valid_identifier(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;

    if (*p == '\0' || (!isalpha(*p) && *p != '_')) {
        return 0;
    }
    for (++p; *p != '\0'; ++p) {
        if (!isalnum(*p) && *p != '_') {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    HexImage image;
    char error[256];
    const char *symbol;
    unsigned i;

    if (argc != 3 || !valid_identifier(argv[2])) {
        fprintf(stderr, "用法：%s firmware.hex c_symbol\n", argv[0]);
        return EXIT_FAILURE;
    }
    symbol = argv[2];
    if (!hex_load_file(argv[1], &image, error, sizeof(error))) {
        fprintf(stderr, "加载失败：%s\n", error);
        return EXIT_FAILURE;
    }

    printf("#include \"picemu/firmware/hex_loader.h\"\n\n");
    printf("const HexImage %s_image = {\n", symbol);
    printf("    .program = {\n");
    for (i = 0; i < PIC10F200_PROGRAM_WORDS; ++i) {
        printf("        0x%03X%s%s", image.program[i],
               i + 1 == PIC10F200_PROGRAM_WORDS ? "" : ",",
               (i % 4 == 3 || i + 1 == PIC10F200_PROGRAM_WORDS)
                   ? "\n" : " ");
    }
    printf("    },\n");
    printf("    .config_word = 0x%03X,\n",
           image.config_present ? image.config_word : 0x0FFFu);
    printf("    .config_present = true\n");
    printf("};\n");
    return EXIT_SUCCESS;
}
