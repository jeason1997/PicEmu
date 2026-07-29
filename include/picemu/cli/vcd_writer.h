#ifndef VCD_WRITER_H
#define VCD_WRITER_H

#include "picemu/core/pic10_cpu.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    bool active;
} VcdWriter;

bool vcd_open(VcdWriter *writer, const char *path, const Pic10Cpu *cpu);
void vcd_sample(VcdWriter *writer, const Pic10Cpu *cpu);
void vcd_close(VcdWriter *writer);

#endif
