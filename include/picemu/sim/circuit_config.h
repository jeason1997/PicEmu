#ifndef CIRCUIT_CONFIG_H
#define CIRCUIT_CONFIG_H

#include "picemu/sim/limits.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CIRCUIT_TEXT_LENGTH 64u
#define CIRCUIT_VALUE_LENGTH 64u
#define CIRCUIT_MAX_PARTS SIM_MAX_PARTS
#define CIRCUIT_MAX_CONNECTIONS SIM_MAX_CONNECTIONS

typedef struct {
    char key[CIRCUIT_TEXT_LENGTH];
    char value[CIRCUIT_VALUE_LENGTH];
} CircuitProperty;

typedef struct {
    char id[CIRCUIT_TEXT_LENGTH];
    char type[CIRCUIT_TEXT_LENGTH];
    int left;
    int top;
    CircuitProperty properties[SIM_MAX_PROPERTIES];
    unsigned property_count;
} CircuitPartConfig;

typedef struct {
    char from[CIRCUIT_TEXT_LENGTH];
    char to[CIRCUIT_TEXT_LENGTH];
    char color[16];
} CircuitConnectionConfig;

typedef struct {
    unsigned version;
    uint32_t clock_hz;
    char firmware[256];
    CircuitPartConfig parts[CIRCUIT_MAX_PARTS];
    unsigned part_count;
    CircuitConnectionConfig connections[CIRCUIT_MAX_CONNECTIONS];
    unsigned connection_count;
} CircuitConfig;

/*
 * 读取与Wokwi diagram.json相似的子集：
 * version、firmware、parts和connections。未知字段会被安全忽略。
 */
bool circuit_config_load(const char *path, CircuitConfig *config,
                         char *error, size_t error_size);
const char *circuit_part_get(const CircuitPartConfig *part,
                             const char *key, const char *fallback);
bool circuit_part_get_bool(const CircuitPartConfig *part,
                           const char *key, bool fallback);
long circuit_part_get_long(const CircuitPartConfig *part,
                           const char *key, long fallback);

#endif
