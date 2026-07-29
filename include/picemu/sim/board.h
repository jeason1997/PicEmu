#ifndef SIM_BOARD_H
#define SIM_BOARD_H

#include "picemu/sim/device.h"
#include "picemu/sim/limits.h"
#include "picemu/sim/mcu.h"

#include <stdbool.h>

typedef enum {
    SIM_ENDPOINT_MCU_PIN,
    SIM_ENDPOINT_DEVICE_PIN
} SimEndpointType;

typedef struct {
    SimEndpointType type;
    union {
        unsigned mcu_pin;
        struct {
            SimDevice *device;
            unsigned pin;
        } device;
    } target;
} SimEndpoint;

typedef struct {
    const char *name;
    SimEndpoint endpoints[SIM_MAX_ENDPOINTS];
    unsigned endpoint_count;
    SimLevel level;
} SimNet;

typedef struct {
    SimMcu *mcu;
    SimNet nets[SIM_MAX_NETS];
    unsigned net_count;
    SimDevice *devices[SIM_MAX_DEVICES];
    unsigned device_count;
} SimBoard;

void sim_board_init(SimBoard *board, SimMcu *mcu);
int sim_board_add_net(SimBoard *board, const char *name);
bool sim_board_connect_mcu(SimBoard *board, int net, unsigned mcu_pin);
bool sim_board_connect_device(SimBoard *board, int net,
                              SimDevice *device, unsigned device_pin);
bool sim_board_merge_nets(SimBoard *board, int keep, int remove);
void sim_board_reset(SimBoard *board);
unsigned sim_board_step(SimBoard *board);
void sim_board_resolve(SimBoard *board);

#endif
