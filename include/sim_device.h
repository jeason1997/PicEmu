#ifndef SIM_DEVICE_H
#define SIM_DEVICE_H

#include <stdbool.h>
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
    void (*tick)(SimDevice *device, uint64_t cycles);
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

typedef struct {
    SimDevice base;
    bool lit;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool active_high;
} SimLed;

void sim_led_init(SimLed *led, const char *name,
                  uint8_t red, uint8_t green, uint8_t blue,
                  bool active_high);

typedef struct {
    SimDevice base;
    bool pressed;
    bool active_low;
} SimButton;

void sim_button_init(SimButton *button, const char *name, bool active_low);
void sim_button_set_pressed(SimButton *button, bool pressed);

typedef struct {
    SimDevice base;
    bool active;
    uint64_t transitions;
} SimBuzzer;

void sim_buzzer_init(SimBuzzer *buzzer, const char *name);

#endif
