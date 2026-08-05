PROJECT_ROOT := ../..
EXAMPLE ?= blink
DEVICE ?= pic10f200
SUPPORTED_DEVICES := pic10f200 pic10f202
ifeq ($(filter $(DEVICE),$(SUPPORTED_DEVICES)),)
$(error DEVICE=$(DEVICE) 不受支持，可选值：$(SUPPORTED_DEVICES))
endif
BUILD_ROOT := build
BUILD_DIR := $(BUILD_ROOT)/$(EXAMPLE)
TARGET := $(BUILD_DIR)/picemu-py32f002a
CONFIG_STAMP := $(BUILD_DIR)/.config-$(DEVICE)
FIRMWARE_HEX := $(PROJECT_ROOT)/examples/$(EXAMPLE)/build/firmware.hex
GENERATED_FIRMWARE := $(BUILD_DIR)/generated/firmware.c
HEX2C := $(BUILD_ROOT)/tools/hex2c

CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE := $(CROSS)size
HOST_CC ?= gcc
OPENOCD ?= openocd
OPENOCD_INTERFACE ?= interface/cmsis-dap.cfg

VENDOR := vendor
CMSIS_INC := $(VENDOR)/Drivers/CMSIS/Include
DEVICE_INC := $(VENDOR)/Drivers/CMSIS/Device/PY32F0xx/Include
LL_INC := $(VENDOR)/Drivers/PY32F0xx_HAL_Driver/Inc
CPU_FLAGS := -mcpu=cortex-m0plus -mthumb
CPPFLAGS := -DPY32F002Ax5 \
	-DPY32_PIC10F202=$(if $(filter pic10f202,$(DEVICE)),1,0) \
	-Iinclude -I$(PROJECT_ROOT)/include \
	-I$(CMSIS_INC) -I$(DEVICE_INC) -I$(LL_INC)
CFLAGS := $(CPU_FLAGS) -std=c11 -O3 -g3 -ffunction-sections \
	-fdata-sections -Wall -Wextra -Wpedantic -Wno-unused-parameter
LDSCRIPT := $(VENDOR)/Core/Startup/py32f002a_32k_4k.ld
LDFLAGS := $(CPU_FLAGS) -nostdlib -T$(LDSCRIPT) -Wl,--gc-sections \
	-Wl,-Map,$(TARGET).map

OBJECTS := \
	$(BUILD_DIR)/obj/port/main.o \
	$(BUILD_DIR)/obj/vendor/system_py32f0xx.o \
	$(BUILD_DIR)/obj/vendor/py32f0xx_ll_utils.o \
	$(BUILD_DIR)/obj/vendor/startup_py32f002xx.o \
	$(BUILD_DIR)/obj/generated/firmware.o \
	$(BUILD_DIR)/obj/core/pic_device.o \
	$(BUILD_DIR)/obj/core/pic10_cpu.o \
	$(BUILD_DIR)/obj/core/gpio_bridge.o
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all clean flash reset check host-test FORCE

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex
	$(SIZE) $(TARGET).elf

$(CONFIG_STAMP):
	@mkdir -p $(dir $@)
	@rm -f $(BUILD_DIR)/.config-*
	@touch $@

$(OBJECTS): $(CONFIG_STAMP)

$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/obj/port/main.o: src/main.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/vendor/system_py32f0xx.o: \
		$(VENDOR)/Core/System/system_py32f0xx.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/vendor/py32f0xx_ll_utils.o: \
		$(VENDOR)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_ll_utils.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/vendor/startup_py32f002xx.o: \
		$(VENDOR)/Core/Startup/startup_py32f002xx.s
	@mkdir -p $(dir $@)
	$(CC) $(CPU_FLAGS) -x assembler-with-cpp -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/generated/firmware.o: $(GENERATED_FIRMWARE)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/core/%.o: $(PROJECT_ROOT)/src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/obj/core/gpio_bridge.o: $(PROJECT_ROOT)/src/platform/gpio_bridge.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(HEX2C): tools/hex2c.c $(PROJECT_ROOT)/src/firmware/hex_loader.c
	@mkdir -p $(dir $@)
	$(HOST_CC) -I$(PROJECT_ROOT)/include -std=c11 -O2 \
		-Wall -Wextra -Wpedantic $^ -o $@

$(GENERATED_FIRMWARE): FORCE $(HEX2C)
	$(MAKE) -C $(PROJECT_ROOT)/examples/$(EXAMPLE) firmware
	@mkdir -p $(dir $@)
	$(HEX2C) $(FIRMWARE_HEX) pic_firmware > $@.tmp
	mv $@.tmp $@

check: $(GENERATED_FIRMWARE)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only src/main.c \
		$(GENERATED_FIRMWARE) $(PROJECT_ROOT)/src/core/pic_device.c \
		$(PROJECT_ROOT)/src/core/pic10_cpu.c \
		$(PROJECT_ROOT)/src/platform/gpio_bridge.c

host-test: $(GENERATED_FIRMWARE)
	@mkdir -p $(BUILD_DIR)/host
	$(HOST_CC) -Iinclude -I$(PROJECT_ROOT)/include -std=c11 \
		-Wall -Wextra -Wpedantic tests/test_firmware.c \
		$(GENERATED_FIRMWARE) $(PROJECT_ROOT)/src/firmware/hex_loader.c \
		-o $(BUILD_DIR)/host/test_firmware
	$(BUILD_DIR)/host/test_firmware $(FIRMWARE_HEX)

flash: all
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
		-c "set CHIPNAME py32f002a; set FLASH_SIZE 0x8000; set WORKAREASIZE 0x1000" \
		-f $(VENDOR)/Core/OpenOCD/py32f002a_target.cfg \
		-c "program $(TARGET).elf verify reset exit"

reset:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
		-c "set CHIPNAME py32f002a; set FLASH_SIZE 0x8000; set WORKAREASIZE 0x1000" \
		-f $(VENDOR)/Core/OpenOCD/py32f002a_target.cfg \
		-c "init; reset run; shutdown"

clean:
	rm -rf $(BUILD_ROOT)

-include $(DEPFILES)

FORCE:
