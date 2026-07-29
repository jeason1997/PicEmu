#include "picemu/sim/board.h"

#include <string.h>

static SimLevel combine(SimLevel current, SimLevel driver)
{
    if (driver == SIM_LEVEL_Z) {
        return current;
    }
    if (driver == SIM_LEVEL_CONFLICT) {
        return SIM_LEVEL_CONFLICT;
    }
    if (current == SIM_LEVEL_Z) {
        return driver;
    }
    return current == driver ? current : SIM_LEVEL_CONFLICT;
}

void sim_board_init(SimBoard *board, SimMcu *mcu)
{
    memset(board, 0, sizeof(*board));
    board->mcu = mcu;
}

int sim_board_add_net(SimBoard *board, const char *name)
{
    SimNet *net;

    if (board->net_count >= SIM_MAX_NETS) {
        return -1;
    }
    net = &board->nets[board->net_count];
    memset(net, 0, sizeof(*net));
    net->name = name;
    net->level = SIM_LEVEL_Z;
    return (int)board->net_count++;
}

static bool add_endpoint(SimBoard *board, int index, SimEndpoint endpoint)
{
    SimNet *net;

    if (index < 0 || (unsigned)index >= board->net_count) {
        return false;
    }
    net = &board->nets[index];
    if (net->endpoint_count >= SIM_MAX_ENDPOINTS) {
        return false;
    }
    net->endpoints[net->endpoint_count++] = endpoint;
    return true;
}

bool sim_board_connect_mcu(SimBoard *board, int net, unsigned mcu_pin)
{
    SimEndpoint endpoint = {.type = SIM_ENDPOINT_MCU_PIN};
    endpoint.target.mcu_pin = mcu_pin;
    return mcu_pin < sim_mcu_pin_count(board->mcu) &&
           add_endpoint(board, net, endpoint);
}

bool sim_board_connect_device(SimBoard *board, int net,
                              SimDevice *device, unsigned device_pin)
{
    SimEndpoint endpoint = {.type = SIM_ENDPOINT_DEVICE_PIN};
    unsigned i;
    bool known = false;

    if (device == NULL || device_pin >= device->pin_count) {
        return false;
    }
    for (i = 0; i < board->device_count; ++i) {
        known |= board->devices[i] == device;
    }
    if (!known) {
        if (board->device_count >= SIM_MAX_DEVICES) {
            return false;
        }
        board->devices[board->device_count++] = device;
    }
    endpoint.target.device.device = device;
    endpoint.target.device.pin = device_pin;
    return add_endpoint(board, net, endpoint);
}

bool sim_board_merge_nets(SimBoard *board, int keep, int remove)
{
    SimNet *target;
    SimNet *source;
    unsigned i;

    if (keep < 0 || remove < 0 || keep == remove ||
        (unsigned)keep >= board->net_count ||
        (unsigned)remove >= board->net_count) return false;
    target = &board->nets[keep];
    source = &board->nets[remove];
    if (target->endpoint_count + source->endpoint_count >
        SIM_MAX_ENDPOINTS) return false;
    for (i = 0; i < source->endpoint_count; ++i) {
        target->endpoints[target->endpoint_count++] = source->endpoints[i];
    }
    source->endpoint_count = 0;
    source->level = SIM_LEVEL_Z;
    return true;
}

void sim_board_resolve(SimBoard *board)
{
    unsigned n;

    for (n = 0; n < board->net_count; ++n) {
        SimNet *net = &board->nets[n];
        SimLevel resolved = SIM_LEVEL_Z;
        unsigned e;

        for (e = 0; e < net->endpoint_count; ++e) {
            SimEndpoint *endpoint = &net->endpoints[e];
            SimLevel drive = SIM_LEVEL_Z;

            if (endpoint->type == SIM_ENDPOINT_MCU_PIN) {
                drive = sim_mcu_pin_drive(board->mcu,
                                          endpoint->target.mcu_pin);
            } else {
                drive = endpoint->target.device.device
                    ->drive[endpoint->target.device.pin];
            }
            resolved = combine(resolved, drive);
        }
        net->level = resolved;

        for (e = 0; e < net->endpoint_count; ++e) {
            SimEndpoint *endpoint = &net->endpoints[e];
            if (endpoint->type == SIM_ENDPOINT_MCU_PIN) {
                sim_mcu_set_pin_input(board->mcu,
                                      endpoint->target.mcu_pin, resolved);
            } else {
                SimDevice *device = endpoint->target.device.device;
                unsigned pin = endpoint->target.device.pin;
                if (device->observed[pin] != resolved) {
                    device->observed[pin] = resolved;
                    if (device->ops != NULL &&
                        device->ops->pin_changed != NULL) {
                        device->ops->pin_changed(device, pin, resolved);
                    }
                }
            }
        }
    }
}

void sim_board_reset(SimBoard *board)
{
    unsigned i;

    sim_mcu_reset(board->mcu);
    for (i = 0; i < board->device_count; ++i) {
        SimDevice *device = board->devices[i];
        if (device->ops != NULL && device->ops->reset != NULL) {
            device->ops->reset(device);
        }
    }
    sim_board_resolve(board);
}

unsigned sim_board_step(SimBoard *board)
{
    unsigned cycles;
    unsigned i;

    sim_board_resolve(board);
    cycles = sim_mcu_step(board->mcu);
    /*
     * 先推进外设时间，再解析本条指令造成的引脚边沿。这样蜂鸣器等测量
     * 边沿间隔的设备会把当前指令周期计入刚结束的电平持续时间。
     */
    for (i = 0; i < board->device_count; ++i) {
        SimDevice *device = board->devices[i];
        if (device->ops != NULL && device->ops->tick != NULL) {
            device->ops->tick(device, cycles,
                              board->mcu->cycles_per_second);
        }
    }
    sim_board_resolve(board);
    return cycles;
}
