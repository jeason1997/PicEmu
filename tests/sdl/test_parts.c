#include "parts/registry.h"
#include "circuit/sdl_circuit.h"
#include "picemu/sim/devices/button.h"
#include "picemu/sim/devices/led.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "%s:%d: 检查失败：%s\n",                        \
                    __FILE__, __LINE__, #condition);                         \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void set_property(CircuitPartConfig *config, const char *key,
                         const char *value)
{
    CircuitProperty *property =
        &config->properties[config->property_count++];
    snprintf(property->key, sizeof(property->key), "%s", key);
    snprintf(property->value, sizeof(property->value), "%s", value);
}

static CircuitPartConfig part_config(const char *id, const char *type)
{
    CircuitPartConfig config;
    memset(&config, 0, sizeof(config));
    snprintf(config.id, sizeof(config.id), "%s", id);
    snprintf(config.type, sizeof(config.type), "%s", type);
    config.left = 100;
    config.top = 100;
    return config;
}

static void test_led_factory(void)
{
    CircuitPartConfig config = part_config("led1", "led");
    SdlPart part;
    SimLed *led;
    char error[128];

    set_property(&config, "color", "green");
    CHECK(sdl_part_create(&part, &config, error, sizeof(error)));
    CHECK(part.device != NULL);
    CHECK(!part.is_mcu);
    led = part.device->state;
    CHECK(led->green == 255 && led->red == 45);
    sdl_part_destroy(&part);
    CHECK(part.device == NULL);
}

static void test_button_mouse(void)
{
    CircuitPartConfig config = part_config("key", "pushbutton");
    SdlPart part;
    SimButton *button;
    char error[128];

    set_property(&config, "activeLow", "false");
    CHECK(sdl_part_create(&part, &config, error, sizeof(error)));
    button = part.device->state;
    CHECK(!button->active_low);
    sdl_part_mouse(&part, part.x, part.y, true);
    CHECK(button->pressed);
    sdl_part_mouse(&part, 0, 0, false);
    CHECK(!button->pressed);
    sdl_part_destroy(&part);
}

static void test_pic_and_unknown_factory(void)
{
    CircuitPartConfig config = part_config("mcu", "pic10f200");
    SdlPart part;
    char error[128];

    CHECK(sdl_part_create(&part, &config, error, sizeof(error)));
    CHECK(part.is_mcu);
    CHECK(part.device == NULL);
    sdl_part_destroy(&part);

    config = part_config("bad", "unknown-device");
    CHECK(!sdl_part_create(&part, &config, error, sizeof(error)));
    CHECK(strstr(error, "unknown-device") != NULL);
}

static void test_shared_endpoint_circuit(void)
{
    CircuitConfig config;
    HexImage image;
    SdlCircuit circuit;
    char error[256];
    unsigned i;

    memset(&config, 0, sizeof(config));
    memset(&image, 0, sizeof(image));
    config.version = 1;
    config.clock_hz = 4000000u;
    config.part_count = 3;
    config.connection_count = 2;
    config.parts[0] = part_config("mcu", "pic10f200");
    config.parts[1] = part_config("led1", "led");
    config.parts[2] = part_config("led2", "led");
    snprintf(config.connections[0].from,
             sizeof(config.connections[0].from), "mcu:GP0");
    snprintf(config.connections[0].to,
             sizeof(config.connections[0].to), "led1:A");
    snprintf(config.connections[1].from,
             sizeof(config.connections[1].from), "led1:A");
    snprintf(config.connections[1].to,
             sizeof(config.connections[1].to), "led2:A");
    for (i = 0; i < PIC10_MAX_PROGRAM_WORDS; ++i) {
        image.program[i] = 0;
    }

    CHECK(sdl_circuit_init(&circuit, &config, &image,
                           error, sizeof(error)));
    CHECK(circuit.wire_count == 2);
    CHECK(circuit.wires[0].net == circuit.wires[1].net);
    CHECK(circuit.board.nets[circuit.wires[0].net].endpoint_count == 3);
    sdl_circuit_destroy(&circuit);
    CHECK(circuit.part_count == 0);
}

int main(void)
{
    test_led_factory();
    test_button_mouse();
    test_pic_and_unknown_factory();
    test_shared_endpoint_circuit();

    if (failures != 0) {
        fprintf(stderr, "SDL器件测试失败：%u项\n", failures);
        return 1;
    }
    printf("SDL器件测试通过。\n");
    return 0;
}
