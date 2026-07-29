#ifndef VCD_WRITER_H
#define VCD_WRITER_H

#include "pic10f200.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    bool active;
} VcdWriter;

bool vcd_open(VcdWriter *writer, const char *path, const Pic10F200 *cpu);
void vcd_sample(VcdWriter *writer, const Pic10F200 *cpu);
void vcd_close(VcdWriter *writer);

#endif
