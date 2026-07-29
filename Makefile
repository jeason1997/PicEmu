CC ?= gcc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

BUILD_DIR := build
OBJECT_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/picemu
SDL_TARGET := $(BUILD_DIR)/picemu-sdl
HEX2C_TARGET := $(BUILD_DIR)/hex2c
UNIT_TEST := $(BUILD_DIR)/tests/test_cpu
SDL_PART_TEST := $(BUILD_DIR)/tests/test_sdl_parts
EXAMPLE ?= button

# 每组源文件对应一个明确的功能模块。build/obj会镜像源目录结构，
# 例如src/core/pic10_cpu.c生成build/obj/src/core/pic10_cpu.o。
CORE_SOURCES := \
	src/core/pic_device.c \
	src/core/pic10_cpu.c \
	src/core/disassembler.c \
	src/firmware/hex_loader.c

CLI_SOURCES := \
	src/cli/main.c \
	src/cli/vcd_writer.c

SIM_SOURCES := \
	src/sim/device.c \
	src/sim/mcu.c \
	src/sim/mcu/pic10.c \
	src/sim/devices/led.c \
	src/sim/devices/button.c \
	src/sim/devices/buzzer.c \
	src/sim/board.c

PLATFORM_SOURCES := src/platform/gpio_bridge.c
CONFIG_SOURCES := src/sim/config/circuit_config.c

SDL_FRONTEND_SOURCES := \
	frontends/sdl/app/main.c \
	frontends/sdl/circuit/sdl_circuit.c \
	frontends/sdl/common/sdl_text.c \
	frontends/sdl/parts/part.c \
	frontends/sdl/parts/registry.c \
	frontends/sdl/parts/pic10.c \
	frontends/sdl/parts/led.c \
	frontends/sdl/parts/button.c \
	frontends/sdl/parts/buzzer.c

SDL_PART_SOURCES := \
	frontends/sdl/common/sdl_text.c \
	frontends/sdl/parts/part.c \
	frontends/sdl/parts/registry.c \
	frontends/sdl/parts/pic10.c \
	frontends/sdl/parts/led.c \
	frontends/sdl/parts/button.c \
	frontends/sdl/parts/buzzer.c

CLI_OBJECTS := $(addprefix $(OBJECT_DIR)/,$(CORE_SOURCES:.c=.o) \
	$(CLI_SOURCES:.c=.o))
SDL_OBJECTS := $(addprefix $(OBJECT_DIR)/,$(CORE_SOURCES:.c=.o) \
	$(SIM_SOURCES:.c=.o) $(PLATFORM_SOURCES:.c=.o) \
	$(CONFIG_SOURCES:.c=.o) $(SDL_FRONTEND_SOURCES:.c=.o))
UNIT_TEST_OBJECTS := $(addprefix $(OBJECT_DIR)/,\
	tests/unit/test_cpu.o $(CORE_SOURCES:.c=.o) $(SIM_SOURCES:.c=.o) \
	$(CONFIG_SOURCES:.c=.o))
SDL_PART_TEST_OBJECTS := $(addprefix $(OBJECT_DIR)/,\
	tests/sdl/test_parts.o $(CORE_SOURCES:.c=.o) \
	src/sim/device.o src/sim/mcu.o src/sim/mcu/pic10.o src/sim/board.o \
	src/sim/devices/led.o src/sim/devices/button.o \
	src/sim/devices/buzzer.o src/sim/config/circuit_config.o \
	frontends/sdl/circuit/sdl_circuit.o $(SDL_PART_SOURCES:.c=.o))
HEX2C_OBJECTS := $(addprefix $(OBJECT_DIR)/,\
	tools/hex2c.o src/firmware/hex_loader.o)

ALL_OBJECTS := $(sort $(CLI_OBJECTS) $(SDL_OBJECTS) \
	$(UNIT_TEST_OBJECTS) $(HEX2C_OBJECTS))
ALL_OBJECTS += $(SDL_PART_TEST_OBJECTS)
DEPFILES := $(ALL_OBJECTS:.o=.d)

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

.PHONY: all clean firmware example-firmware run test unit-test sdl-part-test integration-test \
	sdl run-sdl tools stm32 stm32-host-check stm32-host-test

all: $(TARGET)

$(TARGET): $(CLI_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

# 通用规则让新增模块无需再为每个目录复制编译规则。
$(OBJECT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# SDL源文件需要额外的SDL编译参数和前端内部头文件搜索路径。
$(OBJECT_DIR)/frontends/sdl/%.o: frontends/sdl/%.c
	@test -n "$(SDL_LIBS)" || { \
		echo "错误：没有找到SDL2开发库，请安装libsdl2-dev。"; \
		exit 1; \
	}
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Ifrontends/sdl $(CFLAGS) $(SDL_CFLAGS) \
		-MMD -MP -c $< -o $@

$(OBJECT_DIR)/tests/sdl/%.o: tests/sdl/%.c
	@test -n "$(SDL_LIBS)" || { \
		echo "错误：没有找到SDL2开发库，请安装libsdl2-dev。"; \
		exit 1; \
	}
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Ifrontends/sdl $(CFLAGS) $(SDL_CFLAGS) \
		-MMD -MP -c $< -o $@

$(SDL_TARGET): $(SDL_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS) -lm

sdl: $(SDL_TARGET)

run-sdl: sdl example-firmware
	$(SDL_TARGET) examples/$(EXAMPLE)/diagram.json

$(HEX2C_TARGET): $(HEX2C_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

tools: $(HEX2C_TARGET)

$(UNIT_TEST): $(UNIT_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

firmware:
	$(MAKE) -C examples all

example-firmware:
	$(MAKE) -C examples/$(EXAMPLE) firmware

run: all example-firmware
	$(MAKE) -C examples/$(EXAMPLE) run \
		PICEMU="$(abspath $(TARGET))"

unit-test: $(UNIT_TEST)
	$(UNIT_TEST)

$(SDL_PART_TEST): $(SDL_PART_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS) -lm

sdl-part-test: $(SDL_PART_TEST)
	$(SDL_PART_TEST)

integration-test: all firmware
	sh tests/integration/test_examples.sh "$(abspath $(TARGET))"

test: unit-test sdl-part-test integration-test

stm32:
	$(MAKE) -C ports/stm32f103

stm32-host-check:
	$(MAKE) -C ports/stm32f103 host-check

stm32-host-test:
	$(MAKE) -C ports/stm32f103 host-test

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C examples clean

-include $(DEPFILES)
