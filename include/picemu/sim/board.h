#ifndef SIM_BOARD_H
#define SIM_BOARD_H

#include "picemu/core/pic10_cpu.h"
#include "picemu/sim/device.h"

#include <stdbool.h>

#define SIM_BOARD_MAX_NETS 16u
#define SIM_BOARD_MAX_DEVICES 16u
#define SIM_BOARD_MAX_ENDPOINTS 16u

typedef enum {
    SIM_ENDPOINT_PIC_PIN,
    SIM_ENDPOINT_DEVICE_PIN
} SimEndpointType;

typedef struct {
    SimEndpointType type;
    union {
        unsigned pic_pin;
        struct {
            SimDevice *device;
            unsigned pin;
        } device;
    } target;
} SimEndpoint;

typedef struct {
    const char *name;
    SimEndpoint endpoints[SIM_BOARD_MAX_ENDPOINTS];
    unsigned endpoint_count;
    SimLevel level;
} SimNet;

typedef struct {
    Pic10Cpu *cpu;
    SimNet nets[SIM_BOARD_MAX_NETS];
    unsigned net_count;
    SimDevice *devices[SIM_BOARD_MAX_DEVICES];
    unsigned device_count;
} SimBoard;

void sim_board_init(SimBoard *board, Pic10Cpu *cpu);
int sim_board_add_net(SimBoard *board, const char *name);
bool sim_board_connect_pic(SimBoard *board, int net, unsigned pic_pin);
bool sim_board_connect_device(SimBoard *board, int net,
                              SimDevice *device, unsigned device_pin);
void sim_board_reset(SimBoard *board);
Pic10StepResult sim_board_step(SimBoard *board);
void sim_board_resolve(SimBoard *board);

#endif
