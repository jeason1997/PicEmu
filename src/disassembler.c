#include "disassembler.h"

#include "pic10f200.h"

#include <stdio.h>

const char *pic10_register_name(uint8_t address)
{
    static const char *const names[] = {
        "INDF", "TMR0", "PCL", "STATUS", "FSR", "OSCCAL", "GPIO"
    };

    address &= 0x1Fu;
    return address < sizeof(names) / sizeof(names[0])
        ? names[address] : NULL;
}

static void format_file(char *output, size_t size, uint8_t file)
{
    const char *name = pic10_register_name(file);

    if (name != NULL) {
        snprintf(output, size, "%s", name);
    } else {
        snprintf(output, size, "0x%02X", file & 0x1Fu);
    }
}

int pic10_disassemble(uint16_t instruction, char *output, size_t output_size)
{
    char file_text[16];
    uint8_t file;
    unsigned bit;
    unsigned destination;

    if (output == NULL || output_size == 0) {
        return 0;
    }

    instruction &= 0x0FFFu;
    file = (uint8_t)(instruction & 0x1Fu);
    destination = (instruction >> 5) & 1u;
    bit = (instruction >> 5) & 7u;
    format_file(file_text, sizeof(file_text), file);

    if (instruction == 0x000u) {
        snprintf(output, output_size, "NOP");
    } else if (instruction == 0x002u) {
        snprintf(output, output_size, "OPTION");
    } else if (instruction == 0x003u) {
        snprintf(output, output_size, "SLEEP");
    } else if (instruction == 0x004u) {
        snprintf(output, output_size, "CLRWDT");
    } else if ((instruction & 0xFF8u) == 0x000u &&
               (instruction & 7u) >= 5u) {
        snprintf(output, output_size, "TRIS 0x%X", instruction & 7u);
    } else if ((instruction & 0xFE0u) == 0x020u) {
        snprintf(output, output_size, "MOVWF %s", file_text);
    } else if ((instruction & 0xFE0u) == 0x040u) {
        snprintf(output, output_size, "CLRW");
    } else if ((instruction & 0xFE0u) == 0x060u) {
        snprintf(output, output_size, "CLRF %s", file_text);
    } else if ((instruction & 0xFC0u) == 0x080u) {
        snprintf(output, output_size, "SUBWF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x0C0u) {
        snprintf(output, output_size, "DECF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x100u) {
        snprintf(output, output_size, "IORWF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x140u) {
        snprintf(output, output_size, "ANDWF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x180u) {
        snprintf(output, output_size, "XORWF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x1C0u) {
        snprintf(output, output_size, "ADDWF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x200u) {
        snprintf(output, output_size, "MOVF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x240u) {
        snprintf(output, output_size, "COMF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x280u) {
        snprintf(output, output_size, "INCF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x2C0u) {
        snprintf(output, output_size, "DECFSZ %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x300u) {
        snprintf(output, output_size, "RRF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x340u) {
        snprintf(output, output_size, "RLF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x380u) {
        snprintf(output, output_size, "SWAPF %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xFC0u) == 0x3C0u) {
        snprintf(output, output_size, "INCFSZ %s,%c", file_text,
                 destination ? 'F' : 'W');
    } else if ((instruction & 0xF00u) == 0x400u) {
        snprintf(output, output_size, "BCF %s,%u", file_text, bit);
    } else if ((instruction & 0xF00u) == 0x500u) {
        snprintf(output, output_size, "BSF %s,%u", file_text, bit);
    } else if ((instruction & 0xF00u) == 0x600u) {
        snprintf(output, output_size, "BTFSC %s,%u", file_text, bit);
    } else if ((instruction & 0xF00u) == 0x700u) {
        snprintf(output, output_size, "BTFSS %s,%u", file_text, bit);
    } else if ((instruction & 0xF00u) == 0x800u) {
        snprintf(output, output_size, "RETLW 0x%02X",
                 instruction & 0xFFu);
    } else if ((instruction & 0xF00u) == 0x900u) {
        snprintf(output, output_size, "CALL 0x%02X",
                 instruction & 0xFFu);
    } else if ((instruction & 0xE00u) == 0xA00u) {
        snprintf(output, output_size, "GOTO 0x%03X",
                 instruction & 0x1FFu);
    } else if ((instruction & 0xF00u) == 0xC00u) {
        snprintf(output, output_size, "MOVLW 0x%02X",
                 instruction & 0xFFu);
    } else if ((instruction & 0xF00u) == 0xD00u) {
        snprintf(output, output_size, "IORLW 0x%02X",
                 instruction & 0xFFu);
    } else if ((instruction & 0xF00u) == 0xE00u) {
        snprintf(output, output_size, "ANDLW 0x%02X",
                 instruction & 0xFFu);
    } else if ((instruction & 0xF00u) == 0xF00u) {
        snprintf(output, output_size, "XORLW 0x%02X",
                 instruction & 0xFFu);
    } else {
        snprintf(output, output_size, ".WORD 0x%03X", instruction);
        return 0;
    }

    return 1;
}
