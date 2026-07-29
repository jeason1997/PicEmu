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

static SimLevel pic_drive(const Pic10Cpu *cpu, unsigned pin)
{
    if (pin >= cpu->device->gpio_count ||
        (cpu->device->pins[pin].capabilities & PIC_PIN_CAP_OUTPUT) == 0 ||
        (cpu->tris_gpio & (1u << pin)) != 0) {
        return SIM_LEVEL_Z;
    }
    return (cpu->gpio_latch & (1u << pin)) != 0
        ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW;
}

void sim_board_init(SimBoard *board, Pic10Cpu *cpu)
{
    memset(board, 0, sizeof(*board));
    board->cpu = cpu;
}

int sim_board_add_net(SimBoard *board, const char *name)
{
    SimNet *net;

    if (board->net_count >= SIM_BOARD_MAX_NETS) {
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
    if (net->endpoint_count >= SIM_BOARD_MAX_ENDPOINTS) {
        return false;
    }
    net->endpoints[net->endpoint_count++] = endpoint;
    return true;
}

bool sim_board_connect_pic(SimBoard *board, int net, unsigned pic_pin)
{
    SimEndpoint endpoint = {.type = SIM_ENDPOINT_PIC_PIN};
    endpoint.target.pic_pin = pic_pin;
    return pic_pin < board->cpu->device->gpio_count &&
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
        if (board->device_count >= SIM_BOARD_MAX_DEVICES) {
            return false;
        }
        board->devices[board->device_count++] = device;
    }
    endpoint.target.device.device = device;
    endpoint.target.device.pin = device_pin;
    return add_endpoint(board, net, endpoint);
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

            if (endpoint->type == SIM_ENDPOINT_PIC_PIN) {
                drive = pic_drive(board->cpu, endpoint->target.pic_pin);
            } else {
                drive = endpoint->target.device.device
                    ->drive[endpoint->target.device.pin];
            }
            resolved = combine(resolved, drive);
        }
        net->level = resolved;

        for (e = 0; e < net->endpoint_count; ++e) {
            SimEndpoint *endpoint = &net->endpoints[e];
            if (endpoint->type == SIM_ENDPOINT_PIC_PIN) {
                bool driven = resolved != SIM_LEVEL_Z;
                bool high = resolved == SIM_LEVEL_HIGH;
                pic10_drive_pin(board->cpu, endpoint->target.pic_pin,
                                    driven, high);
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

    pic10_reset(board->cpu, PIC10_RESET_MCLR);
    for (i = 0; i < board->device_count; ++i) {
        SimDevice *device = board->devices[i];
        if (device->ops != NULL && device->ops->reset != NULL) {
            device->ops->reset(device);
        }
    }
    sim_board_resolve(board);
}

Pic10StepResult sim_board_step(SimBoard *board)
{
    Pic10StepResult result;
    unsigned i;

    sim_board_resolve(board);
    result = pic10_step(board->cpu);
    /*
     * 先推进外设时间，再解析本条指令造成的引脚边沿。这样蜂鸣器等测量
     * 边沿间隔的设备会把当前指令周期计入刚结束的电平持续时间。
     */
    for (i = 0; i < board->device_count; ++i) {
        SimDevice *device = board->devices[i];
        if (device->ops != NULL && device->ops->tick != NULL) {
            device->ops->tick(device, result.instruction_cycles);
        }
    }
    sim_board_resolve(board);
    return result;
}
