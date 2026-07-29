CC ?= gcc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

BUILD_DIR := build
OBJECT_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/picemu
SDL_TARGET := $(BUILD_DIR)/picemu-sdl
HEX2C_TARGET := $(BUILD_DIR)/hex2c

CORE_SOURCES := src/hex_loader.c src/pic_device.c src/pic10f200.c \
	src/disassembler.c
CLI_SOURCES := src/main.c src/vcd_writer.c
SIM_SOURCES := src/sim_device.c src/sim_led.c src/sim_button.c \
	src/sim_buzzer.c src/sim_board.c src/pic_platform.c
SDL_SUPPORT_SOURCE := src/circuit_config.c
SOURCES := $(CORE_SOURCES) $(CLI_SOURCES)
OBJECTS := $(patsubst src/%.c,$(OBJECT_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)
SIM_OBJECTS := $(patsubst src/%.c,$(OBJECT_DIR)/%.o,$(SIM_SOURCES))
SDL_FRONTEND_SOURCES := frontends/sdl/main.c \
	frontends/sdl/sdl_circuit.c frontends/sdl/sdl_text.c \
	frontends/sdl/sdl_part_pic10f200.c frontends/sdl/sdl_part_led.c \
	frontends/sdl/sdl_part_button.c frontends/sdl/sdl_part_buzzer.c
SDL_FRONTEND_OBJECTS := $(patsubst frontends/sdl/%.c,\
	$(OBJECT_DIR)/sdl_%.o,$(SDL_FRONTEND_SOURCES))
SDL_SUPPORT_OBJECT := $(OBJECT_DIR)/circuit_config.o
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

UNIT_TEST := $(BUILD_DIR)/tests/test_cpu
UNIT_TEST_OBJECTS := $(OBJECT_DIR)/test_cpu.o \
	$(OBJECT_DIR)/pic10f200.o $(OBJECT_DIR)/disassembler.o \
	$(OBJECT_DIR)/hex_loader.o $(OBJECT_DIR)/pic_device.o \
	$(OBJECT_DIR)/sim_device.o $(OBJECT_DIR)/sim_led.o \
	$(OBJECT_DIR)/sim_button.o $(OBJECT_DIR)/sim_buzzer.o \
	$(OBJECT_DIR)/sim_board.o

.PHONY: all clean firmware run test unit-test sdl run-sdl tools

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(OBJECT_DIR)/%.o: src/%.c | $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJECT_DIR)/test_cpu.o: test/unit/test_cpu.c | $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJECT_DIR)/sdl_%.o: frontends/sdl/%.c | $(OBJECT_DIR)
	@test -n "$(SDL_LIBS)" || { \
		echo "错误：没有找到SDL2开发库，请安装libsdl2-dev。"; \
		exit 1; \
	}
	$(CC) $(CPPFLAGS) -Ifrontends/sdl $(CFLAGS) $(SDL_CFLAGS) \
		-MMD -MP -c $< -o $@

$(SDL_TARGET): $(SDL_FRONTEND_OBJECTS) $(SDL_SUPPORT_OBJECT) \
		$(CORE_SOURCES:src/%.c=$(OBJECT_DIR)/%.o) $(SIM_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS) -lm

sdl: $(SDL_TARGET)

run-sdl: sdl firmware
	$(SDL_TARGET) circuits/blink.json

$(OBJECT_DIR)/hex2c.o: tools/hex2c.c | $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(HEX2C_TARGET): $(OBJECT_DIR)/hex2c.o $(OBJECT_DIR)/hex_loader.o \
		| $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

tools: $(HEX2C_TARGET)

$(UNIT_TEST): $(UNIT_TEST_OBJECTS) | $(BUILD_DIR)/tests
	$(CC) $(CFLAGS) $(UNIT_TEST_OBJECTS) -o $@

$(BUILD_DIR) $(OBJECT_DIR) $(BUILD_DIR)/tests:
	mkdir -p $@

# PIC 固件由 test/Makefile 管理，这里提供一个方便的转发入口。
firmware:
	$(MAKE) -C test firmware

run: all firmware
	$(TARGET) test/build/blink.hex --cycles 3500000

unit-test: $(UNIT_TEST)
	$(UNIT_TEST)

test: all unit-test
	$(MAKE) -C test test PICEMU="$(abspath $(TARGET))"

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C test clean

-include $(DEPFILES) $(SIM_OBJECTS:.o=.d) $(OBJECT_DIR)/test_cpu.d \
	$(SDL_FRONTEND_OBJECTS:.o=.d) $(SDL_SUPPORT_OBJECT:.o=.d) \
	$(OBJECT_DIR)/hex2c.d
