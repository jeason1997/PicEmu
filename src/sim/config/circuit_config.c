#include "picemu/sim/circuit_config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOKEN_ERROR, TOKEN_EOF, TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_TRUE, TOKEN_FALSE,
    TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_COLON, TOKEN_COMMA
} TokenType;

typedef struct {
    const char *cursor;
    unsigned line;
    TokenType type;
    char text[256];
    long number;
} JsonReader;

static void next_token(JsonReader *reader)
{
    const char *p = reader->cursor;
    size_t length = 0;

    while (*p != '\0' && isspace((unsigned char)*p)) {
        if (*p++ == '\n') {
            ++reader->line;
        }
    }
    reader->text[0] = '\0';
    reader->cursor = p + (*p != '\0');
    switch (*p) {
    case '\0': reader->type = TOKEN_EOF; reader->cursor = p; return;
    case '{': reader->type = TOKEN_LBRACE; return;
    case '}': reader->type = TOKEN_RBRACE; return;
    case '[': reader->type = TOKEN_LBRACKET; return;
    case ']': reader->type = TOKEN_RBRACKET; return;
    case ':': reader->type = TOKEN_COLON; return;
    case ',': reader->type = TOKEN_COMMA; return;
    case '"':
        ++p;
        while (*p != '\0' && *p != '"') {
            char value = *p++;
            if (value == '\\') {
                value = *p++;
                if (value == 'n') value = '\n';
                else if (value == 't') value = '\t';
                else if (value != '"' && value != '\\' && value != '/') {
                    reader->type = TOKEN_ERROR;
                    return;
                }
            }
            if (length + 1 < sizeof(reader->text)) {
                reader->text[length++] = value;
            }
        }
        if (*p != '"') {
            reader->type = TOKEN_ERROR;
            return;
        }
        reader->text[length] = '\0';
        reader->cursor = p + 1;
        reader->type = TOKEN_STRING;
        return;
    default:
        if (*p == '-' || isdigit((unsigned char)*p)) {
            char *end;
            errno = 0;
            reader->number = strtol(p, &end, 10);
            if (errno != 0 || end == p) {
                reader->type = TOKEN_ERROR;
                return;
            }
            reader->cursor = end;
            reader->type = TOKEN_NUMBER;
            return;
        }
        if (strncmp(p, "true", 4) == 0) {
            reader->cursor = p + 4;
            reader->type = TOKEN_TRUE;
            return;
        }
        if (strncmp(p, "false", 5) == 0) {
            reader->cursor = p + 5;
            reader->type = TOKEN_FALSE;
            return;
        }
        reader->type = TOKEN_ERROR;
    }
}

static bool accept(JsonReader *reader, TokenType type)
{
    if (reader->type != type) {
        return false;
    }
    next_token(reader);
    return true;
}

static bool skip_value(JsonReader *reader)
{
    TokenType close;

    if (reader->type == TOKEN_STRING || reader->type == TOKEN_NUMBER ||
        reader->type == TOKEN_TRUE || reader->type == TOKEN_FALSE) {
        next_token(reader);
        return true;
    }
    if (reader->type != TOKEN_LBRACE && reader->type != TOKEN_LBRACKET) {
        return false;
    }
    close = reader->type == TOKEN_LBRACE ? TOKEN_RBRACE : TOKEN_RBRACKET;
    next_token(reader);
    while (reader->type != close && reader->type != TOKEN_EOF) {
        if (reader->type == TOKEN_STRING && close == TOKEN_RBRACE) {
            next_token(reader);
            if (!accept(reader, TOKEN_COLON)) return false;
        }
        if (!skip_value(reader)) return false;
        if (reader->type == TOKEN_COMMA) next_token(reader);
        else if (reader->type != close) return false;
    }
    return accept(reader, close);
}

static void copy_text(char *target, size_t size, const char *source)
{
    if (size > 0) {
        size_t length = strlen(source);
        if (length >= size) length = size - 1;
        memcpy(target, source, length);
        target[length] = '\0';
    }
}

static bool parse_attrs(JsonReader *reader, CircuitPartConfig *part)
{
    if (!accept(reader, TOKEN_LBRACE)) return false;
    while (reader->type != TOKEN_RBRACE) {
        char key[64];
        if (reader->type != TOKEN_STRING) return false;
        copy_text(key, sizeof(key), reader->text);
        next_token(reader);
        if (!accept(reader, TOKEN_COLON)) return false;
        if (reader->type == TOKEN_STRING ||
            reader->type == TOKEN_NUMBER ||
            reader->type == TOKEN_TRUE ||
            reader->type == TOKEN_FALSE) {
            CircuitProperty *property =
                part->property_count < SIM_MAX_PROPERTIES
                    ? &part->properties[part->property_count++] : NULL;
            if (property == NULL) return false;
            copy_text(property->key, sizeof(property->key), key);
            if (reader->type == TOKEN_STRING) {
                copy_text(property->value, sizeof(property->value),
                          reader->text);
            } else if (reader->type == TOKEN_NUMBER) {
                snprintf(property->value, sizeof(property->value),
                         "%ld", reader->number);
            } else {
                copy_text(property->value, sizeof(property->value),
                          reader->type == TOKEN_TRUE ? "true" : "false");
            }
            next_token(reader);
        } else if (!skip_value(reader)) {
            return false;
        }
        if (reader->type == TOKEN_COMMA) next_token(reader);
        else if (reader->type != TOKEN_RBRACE) return false;
    }
    return accept(reader, TOKEN_RBRACE);
}

static bool parse_part(JsonReader *reader, CircuitPartConfig *part)
{
    memset(part, 0, sizeof(*part));
    if (!accept(reader, TOKEN_LBRACE)) return false;
    while (reader->type != TOKEN_RBRACE) {
        char key[64];
        if (reader->type != TOKEN_STRING) return false;
        copy_text(key, sizeof(key), reader->text);
        next_token(reader);
        if (!accept(reader, TOKEN_COLON)) return false;
        if ((strcmp(key, "id") == 0 || strcmp(key, "type") == 0) &&
            reader->type == TOKEN_STRING) {
            copy_text(strcmp(key, "id") == 0 ? part->id : part->type,
                      CIRCUIT_TEXT_LENGTH, reader->text);
            next_token(reader);
        } else if ((strcmp(key, "left") == 0 || strcmp(key, "top") == 0) &&
                   reader->type == TOKEN_NUMBER) {
            if (strcmp(key, "left") == 0) part->left = (int)reader->number;
            else part->top = (int)reader->number;
            next_token(reader);
        } else if (strcmp(key, "attrs") == 0) {
            if (!parse_attrs(reader, part)) return false;
        } else if (!skip_value(reader)) {
            return false;
        }
        if (reader->type == TOKEN_COMMA) next_token(reader);
        else if (reader->type != TOKEN_RBRACE) return false;
    }
    return accept(reader, TOKEN_RBRACE) &&
           part->id[0] != '\0' && part->type[0] != '\0';
}

static bool parse_parts(JsonReader *reader, CircuitConfig *config)
{
    if (!accept(reader, TOKEN_LBRACKET)) return false;
    while (reader->type != TOKEN_RBRACKET) {
        if (config->part_count >= CIRCUIT_MAX_PARTS ||
            !parse_part(reader, &config->parts[config->part_count])) {
            return false;
        }
        ++config->part_count;
        if (reader->type == TOKEN_COMMA) next_token(reader);
        else if (reader->type != TOKEN_RBRACKET) return false;
    }
    return accept(reader, TOKEN_RBRACKET);
}

static bool parse_connection(JsonReader *reader,
                             CircuitConnectionConfig *connection)
{
    unsigned item = 0;
    memset(connection, 0, sizeof(*connection));
    if (!accept(reader, TOKEN_LBRACKET)) return false;
    while (reader->type != TOKEN_RBRACKET) {
        if (reader->type == TOKEN_STRING && item < 3) {
            char *target = item == 0 ? connection->from :
                           item == 1 ? connection->to : connection->color;
            size_t size = item == 2 ? sizeof(connection->color) :
                                      CIRCUIT_TEXT_LENGTH;
            copy_text(target, size, reader->text);
            next_token(reader);
        } else if (!skip_value(reader)) {
            return false;
        }
        ++item;
        if (reader->type == TOKEN_COMMA) next_token(reader);
        else if (reader->type != TOKEN_RBRACKET) return false;
    }
    return accept(reader, TOKEN_RBRACKET) && item >= 2;
}

static bool parse_connections(JsonReader *reader, CircuitConfig *config)
{
    if (!accept(reader, TOKEN_LBRACKET)) return false;
    while (reader->type != TOKEN_RBRACKET) {
        if (config->connection_count >= CIRCUIT_MAX_CONNECTIONS ||
            !parse_connection(reader,
                &config->connections[config->connection_count])) {
            return false;
        }
        ++config->connection_count;
        if (reader->type == TOKEN_COMMA) next_token(reader);
        else if (reader->type != TOKEN_RBRACKET) return false;
    }
    return accept(reader, TOKEN_RBRACKET);
}

static bool parse_document(JsonReader *reader, CircuitConfig *config)
{
    if (!accept(reader, TOKEN_LBRACE)) return false;
    while (reader->type != TOKEN_RBRACE) {
        char key[64];
        if (reader->type != TOKEN_STRING) return false;
        copy_text(key, sizeof(key), reader->text);
        next_token(reader);
        if (!accept(reader, TOKEN_COLON)) return false;
        if (strcmp(key, "version") == 0 && reader->type == TOKEN_NUMBER) {
            config->version = (unsigned)reader->number;
            next_token(reader);
        } else if (strcmp(key, "clockHz") == 0 &&
                   reader->type == TOKEN_NUMBER &&
                   reader->number > 0 &&
                   (unsigned long)reader->number <= UINT32_MAX) {
            config->clock_hz = (uint32_t)reader->number;
            next_token(reader);
        } else if (strcmp(key, "firmware") == 0 &&
                   reader->type == TOKEN_STRING) {
            copy_text(config->firmware, sizeof(config->firmware),
                      reader->text);
            next_token(reader);
        } else if (strcmp(key, "parts") == 0) {
            if (!parse_parts(reader, config)) return false;
        } else if (strcmp(key, "connections") == 0) {
            if (!parse_connections(reader, config)) return false;
        } else if (!skip_value(reader)) {
            return false;
        }
        if (reader->type == TOKEN_COMMA) next_token(reader);
        else if (reader->type != TOKEN_RBRACE) return false;
    }
    return accept(reader, TOKEN_RBRACE);
}

bool circuit_config_load(const char *path, CircuitConfig *config,
                         char *error, size_t error_size)
{
    FILE *file;
    char *data;
    long size;
    JsonReader reader;
    bool ok;

    file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(error, error_size, "无法打开电路配置：%s", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        snprintf(error, error_size, "无法读取电路配置大小");
        return false;
    }
    data = malloc((size_t)size + 1);
    if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        snprintf(error, error_size, "读取电路配置失败");
        return false;
    }
    data[size] = '\0';
    fclose(file);

    memset(config, 0, sizeof(*config));
    config->clock_hz = 4000000u;
    reader.cursor = data;
    reader.line = 1;
    next_token(&reader);
    ok = parse_document(&reader, config) && reader.type == TOKEN_EOF;
    if (!ok) {
        snprintf(error, error_size, "JSON格式错误（约第%u行）", reader.line);
    } else if (config->version != 1 || config->part_count == 0) {
        snprintf(error, error_size, "配置必须使用version 1并至少包含一个部件");
        ok = false;
    }
    free(data);
    return ok;
}

const char *circuit_part_get(const CircuitPartConfig *part,
                             const char *key, const char *fallback)
{
    unsigned i;
    for (i = 0; i < part->property_count; ++i) {
        if (strcmp(part->properties[i].key, key) == 0) {
            return part->properties[i].value;
        }
    }
    return fallback;
}

bool circuit_part_get_bool(const CircuitPartConfig *part,
                           const char *key, bool fallback)
{
    const char *value = circuit_part_get(part, key, NULL);
    if (value == NULL) return fallback;
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) return true;
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) return false;
    return fallback;
}

long circuit_part_get_long(const CircuitPartConfig *part,
                           const char *key, long fallback)
{
    const char *value = circuit_part_get(part, key, NULL);
    char *end;
    long result;
    if (value == NULL) return fallback;
    errno = 0;
    result = strtol(value, &end, 0);
    return errno == 0 && end != value && *end == '\0' ? result : fallback;
}
