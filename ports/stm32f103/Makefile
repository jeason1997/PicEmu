PROJECT_ROOT := ../..
EXAMPLE ?= blink
BUILD_ROOT := build
BUILD_DIR := $(BUILD_ROOT)/$(EXAMPLE)
TARGET := $(BUILD_DIR)/picemu-stm32f103
FIRMWARE_HEX := $(PROJECT_ROOT)/examples/$(EXAMPLE)/build/firmware.hex
HEX2C := $(PROJECT_ROOT)/build/hex2c
GENERATED_FIRMWARE := $(BUILD_DIR)/generated/firmware.c

CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE := $(CROSS)size
HOST_CC ?= gcc

CPPFLAGS := -Iinclude -I$(PROJECT_ROOT)/include
CFLAGS := -mcpu=cortex-m3 -mthumb -std=c11 -O3 -g3 \
	-ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic
CFLAGS += $(CFLAGS_EXTRA)
LDFLAGS := -mcpu=cortex-m3 -mthumb -nostartfiles \
	-Tstm32f103.ld -Wl,--gc-sections \
	-Wl,-Map,$(TARGET).map --specs=nano.specs --specs=nosys.specs

PORT_SOURCES := src/startup.c src/main.c
CORE_SOURCES := \
	$(PROJECT_ROOT)/src/core/pic_device.c \
	$(PROJECT_ROOT)/src/core/pic10_cpu.c \
	$(PROJECT_ROOT)/src/platform/gpio_bridge.c

OBJECTS := \
	$(BUILD_DIR)/obj/port/startup.o \
	$(BUILD_DIR)/obj/port/main.o \
	$(BUILD_DIR)/obj/generated/firmware.o \
	$(BUILD_DIR)/obj/core/pic_device.o \
	$(BUILD_DIR)/obj/core/pic10_cpu.o \
	$(BUILD_DIR)/obj/core/gpio_bridge.o
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all clean flash host-check host-test FORCE

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJECTS) stm32f103.ld
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/obj/port/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/generated/firmware.o: $(GENERATED_FIRMWARE)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(HEX2C):
	$(MAKE) -C $(PROJECT_ROOT) tools

$(GENERATED_FIRMWARE): FORCE $(HEX2C)
	$(MAKE) -C $(PROJECT_ROOT)/examples/$(EXAMPLE) firmware
	@mkdir -p $(dir $@)
	$(HEX2C) $(FIRMWARE_HEX) pic_firmware > $@.tmp
	mv $@.tmp $@

$(BUILD_DIR)/obj/core/pic_device.o: $(PROJECT_ROOT)/src/core/pic_device.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/core/pic10_cpu.o: $(PROJECT_ROOT)/src/core/pic10_cpu.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/core/gpio_bridge.o: $(PROJECT_ROOT)/src/platform/gpio_bridge.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# 没有ARM工具链时，至少用宿主GCC检查平台C代码和公共核心的接口。
host-check: $(GENERATED_FIRMWARE)
	$(HOST_CC) $(CPPFLAGS) -DSTM32_HOST_CHECK -std=c11 \
		-Wall -Wextra -Wpedantic -fsyntax-only \
		src/startup.c src/main.c $(GENERATED_FIRMWARE) \
		$(PROJECT_ROOT)/src/core/pic_device.c \
		$(PROJECT_ROOT)/src/core/pic10_cpu.c \
		$(PROJECT_ROOT)/src/platform/gpio_bridge.c

host-test: $(GENERATED_FIRMWARE)
	@mkdir -p $(BUILD_DIR)/host
	$(HOST_CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic \
		tests/test_firmware.c $(GENERATED_FIRMWARE) \
		$(PROJECT_ROOT)/src/firmware/hex_loader.c \
		-o $(BUILD_DIR)/host/test_firmware
	$(BUILD_DIR)/host/test_firmware \
		$(FIRMWARE_HEX)

# 默认OpenOCD配置适用于常见ST-Link和STM32F1目标。
flash: $(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
		-c "program $(TARGET).elf verify reset exit"

clean:
	rm -rf $(BUILD_ROOT)

-include $(DEPFILES)

FORCE:
