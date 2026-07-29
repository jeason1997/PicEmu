CC ?= gcc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

BUILD_DIR := build
OBJECT_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/picemu
SDL_TARGET := $(BUILD_DIR)/picemu-sdl
HEX2C_TARGET := $(BUILD_DIR)/hex2c
UNIT_TEST := $(BUILD_DIR)/tests/test_cpu

# 每组源文件对应一个明确的功能模块。build/obj会镜像源目录结构，
# 例如src/core/pic10f200.c生成build/obj/src/core/pic10f200.o。
CORE_SOURCES := \
	src/core/pic_device.c \
	src/core/pic10f200.c \
	src/core/disassembler.c \
	src/firmware/hex_loader.c

CLI_SOURCES := \
	src/cli/main.c \
	src/cli/vcd_writer.c

SIM_SOURCES := \
	src/sim/device.c \
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
	frontends/sdl/parts/pic10f200.c \
	frontends/sdl/parts/led.c \
	frontends/sdl/parts/button.c \
	frontends/sdl/parts/buzzer.c

CLI_OBJECTS := $(addprefix $(OBJECT_DIR)/,$(CORE_SOURCES:.c=.o) \
	$(CLI_SOURCES:.c=.o))
SDL_OBJECTS := $(addprefix $(OBJECT_DIR)/,$(CORE_SOURCES:.c=.o) \
	$(SIM_SOURCES:.c=.o) $(PLATFORM_SOURCES:.c=.o) \
	$(CONFIG_SOURCES:.c=.o) $(SDL_FRONTEND_SOURCES:.c=.o))
UNIT_TEST_OBJECTS := $(addprefix $(OBJECT_DIR)/,\
	test/unit/test_cpu.o $(CORE_SOURCES:.c=.o) $(SIM_SOURCES:.c=.o))
HEX2C_OBJECTS := $(addprefix $(OBJECT_DIR)/,\
	tools/hex2c.o src/firmware/hex_loader.o)

ALL_OBJECTS := $(sort $(CLI_OBJECTS) $(SDL_OBJECTS) \
	$(UNIT_TEST_OBJECTS) $(HEX2C_OBJECTS))
DEPFILES := $(ALL_OBJECTS:.o=.d)

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

.PHONY: all clean firmware run test unit-test sdl run-sdl tools

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

$(SDL_TARGET): $(SDL_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS) -lm

sdl: $(SDL_TARGET)

run-sdl: sdl firmware
	$(SDL_TARGET) circuits/blink.json

$(HEX2C_TARGET): $(HEX2C_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

tools: $(HEX2C_TARGET)

$(UNIT_TEST): $(UNIT_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

firmware:
	$(MAKE) -C test firmware

run: all firmware
	$(TARGET) test/build/blink.hex --cycles 120000 \
		--events test/button_events.txt

unit-test: $(UNIT_TEST)
	$(UNIT_TEST)

test: all unit-test
	$(MAKE) -C test test PICEMU="$(abspath $(TARGET))"

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C test clean

-include $(DEPFILES)
