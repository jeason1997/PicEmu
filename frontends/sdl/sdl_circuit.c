#include "sdl_circuit.h"

#include <stdio.h>
#include <string.h>

static SdlPart *find_part(SdlCircuit *circuit, const char *id,
                          unsigned *index)
{
    unsigned i;
    for (i = 0; i < circuit->part_count; ++i) {
        if (strcmp(circuit->parts[i].id, id) == 0) {
            if (index != NULL) *index = i;
            return &circuit->parts[i];
        }
    }
    return NULL;
}

static bool split_endpoint(const char *endpoint, char *id, size_t id_size,
                           char *pin, size_t pin_size)
{
    const char *colon = strchr(endpoint, ':');
    size_t length;
    if (colon == NULL || colon == endpoint || colon[1] == '\0') return false;
    length = (size_t)(colon - endpoint);
    if (length >= id_size) return false;
    memcpy(id, endpoint, length);
    id[length] = '\0';
    snprintf(pin, pin_size, "%s", colon + 1);
    return true;
}

static SDL_Color wire_color(const char *name)
{
    if (strcmp(name, "red") == 0) return (SDL_Color){230, 70, 70, 255};
    if (strcmp(name, "green") == 0) return (SDL_Color){55, 205, 90, 255};
    if (strcmp(name, "blue") == 0) return (SDL_Color){60, 130, 235, 255};
    if (strcmp(name, "yellow") == 0) return (SDL_Color){235, 200, 45, 255};
    return (SDL_Color){150, 160, 170, 255};
}

static bool part_pin(SdlPart *part, const char *name,
                     unsigned *pin, SDL_Point *point, bool *is_pic,
                     bool *is_signal)
{
    *is_pic = false;
    *is_signal = true;
    switch (part->type) {
    case SDL_PART_PIC10F200:
        *is_pic = true;
        return sdl_part_pic10f200_pin(part, name, pin, point, is_signal);
    case SDL_PART_LED:
        return sdl_part_led_pin(part, name, pin, point);
    case SDL_PART_BUTTON:
        return sdl_part_button_pin(part, name, pin, point);
    case SDL_PART_BUZZER:
        return sdl_part_buzzer_pin(part, name, pin, point);
    }
    return false;
}

static SimDevice *part_device(SdlPart *part)
{
    switch (part->type) {
    case SDL_PART_LED: return &part->device.led.base;
    case SDL_PART_BUTTON: return &part->device.button.base;
    case SDL_PART_BUZZER: return &part->device.buzzer.base;
    default: return NULL;
    }
}

static bool init_part(SdlPart *part, const CircuitPartConfig *config,
                      char *error, size_t error_size)
{
    memset(part, 0, sizeof(*part));
    snprintf(part->id, sizeof(part->id), "%s", config->id);
    part->x = config->left;
    part->y = config->top;
    if (strcmp(config->type, "pic10f200") == 0) {
        part->type = SDL_PART_PIC10F200;
    } else if (strcmp(config->type, "led") == 0) {
        uint8_t r = 255, g = 45, b = 35;
        part->type = SDL_PART_LED;
        if (strcmp(config->color, "green") == 0) {
            r = 45; g = 255; b = 65;
        } else if (strcmp(config->color, "blue") == 0) {
            r = 45; g = 110; b = 255;
        } else if (strcmp(config->color, "yellow") == 0) {
            r = 255; g = 210; b = 35;
        }
        sim_led_init(&part->device.led, part->id, r, g, b, true);
    } else if (strcmp(config->type, "pushbutton") == 0) {
        part->type = SDL_PART_BUTTON;
        sim_button_init(&part->device.button, part->id, config->active_low);
    } else if (strcmp(config->type, "buzzer") == 0) {
        part->type = SDL_PART_BUZZER;
        sim_buzzer_init(&part->device.buzzer, part->id);
    } else {
        snprintf(error, error_size, "不支持的器件类型：%s", config->type);
        return false;
    }
    return true;
}

bool sdl_circuit_init(SdlCircuit *circuit, const CircuitConfig *config,
                      const HexImage *image, char *error,
                      size_t error_size)
{
    unsigned i;
    unsigned mcu_count = 0;

    memset(circuit, 0, sizeof(*circuit));
    pic10f200_init(&circuit->cpu, image);
    sim_board_init(&circuit->board, &circuit->cpu);
    circuit->part_count = config->part_count;

    for (i = 0; i < config->part_count; ++i) {
        if (!init_part(&circuit->parts[i], &config->parts[i],
                       error, error_size)) return false;
        if (circuit->parts[i].type == SDL_PART_PIC10F200) ++mcu_count;
        if (i > 0 && find_part(circuit, circuit->parts[i].id, NULL) !=
                     &circuit->parts[i]) {
            snprintf(error, error_size, "重复的器件ID：%s",
                     circuit->parts[i].id);
            return false;
        }
    }
    if (mcu_count != 1) {
        snprintf(error, error_size, "当前电路必须包含一个pic10f200主控");
        return false;
    }

    for (i = 0; i < config->connection_count; ++i) {
        const CircuitConnectionConfig *source = &config->connections[i];
        SdlWire *wire = &circuit->wires[circuit->wire_count];
        char id_a[64], id_b[64];
        unsigned pin_a, pin_b;
        bool pic_a, pic_b, signal_a, signal_b;
        SDL_Point unused;
        SdlPart *part_a;
        SdlPart *part_b;

        if (!split_endpoint(source->from, id_a, sizeof(id_a),
                            wire->pin_a, sizeof(wire->pin_a)) ||
            !split_endpoint(source->to, id_b, sizeof(id_b),
                            wire->pin_b, sizeof(wire->pin_b)) ||
            (part_a = find_part(circuit, id_a, &wire->part_a)) == NULL ||
            (part_b = find_part(circuit, id_b, &wire->part_b)) == NULL ||
            !part_pin(part_a, wire->pin_a, &pin_a, &unused,
                      &pic_a, &signal_a) ||
            !part_pin(part_b, wire->pin_b, &pin_b, &unused,
                      &pic_b, &signal_b)) {
            snprintf(error, error_size, "无效连接：%s -> %s",
                     source->from, source->to);
            return false;
        }
        if (!signal_a || !signal_b) {
            /* VDD/VSS目前仅用于绘图，不进入数字GPIO网络。 */
            wire->net = -1;
        } else {
            wire->net = sim_board_add_net(&circuit->board, source->from);
            if (wire->net < 0 || pic_a == pic_b ||
                !(pic_a ? sim_board_connect_pic(&circuit->board,
                                                wire->net, pin_a)
                         : sim_board_connect_device(&circuit->board,
                             wire->net, part_device(part_a), pin_a)) ||
                !(pic_b ? sim_board_connect_pic(&circuit->board,
                                                wire->net, pin_b)
                         : sim_board_connect_device(&circuit->board,
                             wire->net, part_device(part_b), pin_b))) {
                snprintf(error, error_size,
                         "连接必须由一个主控引脚连接一个外设引脚：%s -> %s",
                         source->from, source->to);
                return false;
            }
        }
        wire->color = wire_color(source->color);
        ++circuit->wire_count;
    }
    sim_board_resolve(&circuit->board);
    return true;
}

void sdl_circuit_reset(SdlCircuit *circuit)
{
    sim_board_reset(&circuit->board);
}

void sdl_circuit_step(SdlCircuit *circuit)
{
    sim_board_step(&circuit->board);
}

static void render_part(SDL_Renderer *renderer, const SdlPart *part)
{
    switch (part->type) {
    case SDL_PART_PIC10F200: sdl_part_pic10f200_render(renderer, part); break;
    case SDL_PART_LED: sdl_part_led_render(renderer, part); break;
    case SDL_PART_BUTTON: sdl_part_button_render(renderer, part); break;
    case SDL_PART_BUZZER: sdl_part_buzzer_render(renderer, part); break;
    }
}

void sdl_circuit_render(SDL_Renderer *renderer,
                        const SdlCircuit *circuit, bool running)
{
    unsigned i;
    SDL_Rect state_bar = {0, 0, 960, 45};
    SDL_SetRenderDrawColor(renderer, 18, 22, 27, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, running ? 35 : 80,
                          running ? 115 : 80, running ? 60 : 80, 255);
    SDL_RenderFillRect(renderer, &state_bar);

    for (i = 0; i < circuit->wire_count; ++i) {
        const SdlWire *wire = &circuit->wires[i];
        SdlPart *a = (SdlPart *)&circuit->parts[wire->part_a];
        SdlPart *b = (SdlPart *)&circuit->parts[wire->part_b];
        SDL_Point pa, pb;
        unsigned pin;
        bool is_pic, signal;
        part_pin(a, wire->pin_a, &pin, &pa, &is_pic, &signal);
        part_pin(b, wire->pin_b, &pin, &pb, &is_pic, &signal);
        SDL_SetRenderDrawColor(renderer, wire->color.r, wire->color.g,
                               wire->color.b, 255);
        SDL_RenderDrawLine(renderer, pa.x, pa.y, (pa.x + pb.x) / 2, pa.y);
        SDL_RenderDrawLine(renderer, (pa.x + pb.x) / 2, pa.y,
                           (pa.x + pb.x) / 2, pb.y);
        SDL_RenderDrawLine(renderer, (pa.x + pb.x) / 2, pb.y, pb.x, pb.y);
    }
    for (i = 0; i < circuit->part_count; ++i) {
        render_part(renderer, &circuit->parts[i]);
    }
    SDL_RenderPresent(renderer);
}

void sdl_circuit_mouse(SdlCircuit *circuit, int x, int y, bool pressed)
{
    unsigned i;
    for (i = 0; i < circuit->part_count; ++i) {
        SdlPart *part = &circuit->parts[i];
        if (part->type == SDL_PART_BUTTON &&
            (!pressed || sdl_part_button_hit(part, x, y))) {
            sim_button_set_pressed(&part->device.button, pressed);
        }
    }
    sim_board_resolve(&circuit->board);
}

bool sdl_circuit_buzzer_active(const SdlCircuit *circuit)
{
    unsigned i;
    for (i = 0; i < circuit->part_count; ++i) {
        if (circuit->parts[i].type == SDL_PART_BUZZER &&
            circuit->parts[i].device.buzzer.active) return true;
    }
    return false;
}
