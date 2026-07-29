#ifndef CIRCUIT_CONFIG_H
#define CIRCUIT_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define CIRCUIT_MAX_PARTS 32u
#define CIRCUIT_MAX_CONNECTIONS 64u
#define CIRCUIT_TEXT_LENGTH 64u

typedef struct {
    char id[CIRCUIT_TEXT_LENGTH];
    char type[CIRCUIT_TEXT_LENGTH];
    int left;
    int top;
    char color[16];
    bool active_low;
} CircuitPartConfig;

typedef struct {
    char from[CIRCUIT_TEXT_LENGTH];
    char to[CIRCUIT_TEXT_LENGTH];
    char color[16];
} CircuitConnectionConfig;

typedef struct {
    unsigned version;
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

#endif
