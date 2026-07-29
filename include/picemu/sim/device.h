#ifndef SIM_DEVICE_H
#define SIM_DEVICE_H

#include <stdint.h>

#define SIM_DEVICE_MAX_PINS 8u

typedef enum {
    SIM_LEVEL_LOW,
    SIM_LEVEL_HIGH,
    SIM_LEVEL_Z,
    SIM_LEVEL_CONFLICT
} SimLevel;

typedef struct SimDevice SimDevice;

typedef struct {
    void (*reset)(SimDevice *device);
    void (*tick)(SimDevice *device, uint64_t cycles,
                 uint32_t cycles_per_second);
    void (*pin_changed)(SimDevice *device, unsigned pin, SimLevel level);
} SimDeviceOps;

/*
 * 虚拟设备不直接访问PIC。它只通过引脚驱动/观察网络，因此LED、按键、
 * 蜂鸣器以及未来的屏幕、移位寄存器都可以使用相同接口。
 */
struct SimDevice {
    const char *name;
    const SimDeviceOps *ops;
    void *state;
    unsigned pin_count;
    SimLevel drive[SIM_DEVICE_MAX_PINS];
    SimLevel observed[SIM_DEVICE_MAX_PINS];
};

void sim_device_init(SimDevice *device, const char *name,
                     const SimDeviceOps *ops, void *state,
                     unsigned pin_count);
void sim_device_set_drive(SimDevice *device, unsigned pin, SimLevel level);

#endif
