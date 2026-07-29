#include "circuit/sdl_circuit.h"
#include "parts/registry.h"

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

static int endpoint_net(const SdlCircuit *circuit, unsigned part,
                        const char *pin)
{
    unsigned i;
    for (i = 0; i < circuit->wire_count; ++i) {
        const SdlWire *wire = &circuit->wires[i];
        if ((wire->part_a == part && strcmp(wire->pin_a, pin) == 0) ||
            (wire->part_b == part && strcmp(wire->pin_b, pin) == 0)) {
            return wire->net;
        }
    }
    return -1;
}

static void replace_wire_net(SdlCircuit *circuit, int old_net, int new_net)
{
    unsigned i;
    for (i = 0; i < circuit->wire_count; ++i) {
        if (circuit->wires[i].net == old_net) {
            circuit->wires[i].net = new_net;
        }
    }
}

static bool part_pin(SdlPart *part, const char *name,
                     unsigned *pin, SDL_Point *point, bool *is_pic,
                     bool *is_signal)
{
    *is_pic = part->is_mcu;
    return sdl_part_find_pin(part, name, pin, point, is_signal);
}

bool sdl_circuit_init(SdlCircuit *circuit, const CircuitConfig *config,
                      const HexImage *image, char *error,
                      size_t error_size)
{
    unsigned i;
    unsigned mcu_count = 0;
    const PicDeviceDescription *device = NULL;

    memset(circuit, 0, sizeof(*circuit));
    circuit->part_count = config->part_count;

    for (i = 0; i < config->part_count; ++i) {
        if (!sdl_part_create(&circuit->parts[i], &config->parts[i],
                             error, error_size)) {
            sdl_circuit_destroy(circuit);
            return false;
        }
        if (circuit->parts[i].is_mcu) {
            ++mcu_count;
            device = circuit->parts[i].view_state;
        }
        if (i > 0 && find_part(circuit, circuit->parts[i].id, NULL) !=
                     &circuit->parts[i]) {
            snprintf(error, error_size, "重复的器件ID：%s",
                     circuit->parts[i].id);
            sdl_circuit_destroy(circuit);
            return false;
        }
    }
    if (mcu_count != 1) {
        snprintf(error, error_size,
                 "当前电路必须包含一个pic10f200或pic10f202主控");
        sdl_circuit_destroy(circuit);
        return false;
    }
    pic10_init(&circuit->cpu, image, device);
    sim_pic10_mcu_init(&circuit->mcu_adapter, &circuit->cpu,
                       config->clock_hz);
    sim_board_init(&circuit->board, &circuit->mcu_adapter.base);

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
            sdl_circuit_destroy(circuit);
            return false;
        }
        if (!signal_a || !signal_b) {
            /* VDD/VSS目前仅用于绘图，不进入数字GPIO网络。 */
            wire->net = -1;
        } else {
            int net_a = endpoint_net(circuit, wire->part_a, wire->pin_a);
            int net_b = endpoint_net(circuit, wire->part_b, wire->pin_b);
            bool connect_a = net_a < 0;
            bool connect_b = net_b < 0;

            if (net_a >= 0 && net_b >= 0 && net_a != net_b) {
                if (!sim_board_merge_nets(&circuit->board, net_a, net_b)) {
                    snprintf(error, error_size, "网络合并失败：%s -> %s",
                             source->from, source->to);
                    sdl_circuit_destroy(circuit);
                    return false;
                }
                replace_wire_net(circuit, net_b, net_a);
                net_b = net_a;
            }
            wire->net = net_a >= 0 ? net_a :
                        net_b >= 0 ? net_b :
                        sim_board_add_net(&circuit->board, source->from);

            if (wire->net < 0 ||
                (connect_a &&
                 !(pic_a ? sim_board_connect_mcu(&circuit->board,
                                                 wire->net, pin_a)
                          : sim_board_connect_device(&circuit->board,
                              wire->net, part_a->device, pin_a))) ||
                (connect_b &&
                 !(pic_b ? sim_board_connect_mcu(&circuit->board,
                                                 wire->net, pin_b)
                          : sim_board_connect_device(&circuit->board,
                              wire->net, part_b->device, pin_b)))) {
                snprintf(error, error_size,
                         "无法连接网络端点：%s -> %s",
                         source->from, source->to);
                sdl_circuit_destroy(circuit);
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

unsigned sdl_circuit_step(SdlCircuit *circuit)
{
    return sim_board_step(&circuit->board);
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
        sdl_part_render(renderer, &circuit->parts[i]);
    }
    SDL_RenderPresent(renderer);
}

void sdl_circuit_mouse(SdlCircuit *circuit, int x, int y, bool pressed)
{
    unsigned i;
    for (i = 0; i < circuit->part_count; ++i) {
        sdl_part_mouse(&circuit->parts[i], x, y, pressed);
    }
    sim_board_resolve(&circuit->board);
}

double sdl_circuit_buzzer_frequency(const SdlCircuit *circuit)
{
    unsigned i;
    for (i = 0; i < circuit->part_count; ++i) {
        double frequency = sdl_part_audio_frequency(&circuit->parts[i]);
        if (frequency > 0.0) return frequency;
    }
    return 0.0;
}

void sdl_circuit_destroy(SdlCircuit *circuit)
{
    unsigned i;
    if (circuit == NULL) return;
    for (i = 0; i < circuit->part_count; ++i) {
        sdl_part_destroy(&circuit->parts[i]);
    }
    circuit->part_count = 0;
}
