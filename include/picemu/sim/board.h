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
        struct {
            SimMcu *mcu;
            unsigned pin;
        } mcu;
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
    /* mcu 保留为首颗主控的兼容别名，新代码应使用 mcus/mcu_count。 */
    SimMcu *mcu;
    /*
     * MCU 是器件的一种，不再设置独立的主控数量上限。
     * 一张电路能够容纳多少器件，就能够容纳多少颗 MCU。
     */
    SimMcu *mcus[SIM_MAX_PARTS];
    unsigned mcu_count;
    SimNet nets[SIM_MAX_NETS];
    unsigned net_count;
    SimDevice *devices[SIM_MAX_DEVICES];
    unsigned device_count;
} SimBoard;

void sim_board_init(SimBoard *board, SimMcu *mcu);
bool sim_board_add_mcu(SimBoard *board, SimMcu *mcu);
int sim_board_add_net(SimBoard *board, const char *name);
bool sim_board_connect_mcu(SimBoard *board, int net, unsigned mcu_pin);
bool sim_board_connect_mcu_instance(SimBoard *board, int net,
                                    SimMcu *mcu, unsigned mcu_pin);
bool sim_board_connect_device(SimBoard *board, int net,
                              SimDevice *device, unsigned device_pin);
bool sim_board_merge_nets(SimBoard *board, int keep, int remove);
void sim_board_reset(SimBoard *board);
unsigned sim_board_step(SimBoard *board);
void sim_board_resolve(SimBoard *board);

#endif
