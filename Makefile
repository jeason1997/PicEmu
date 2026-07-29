CC ?= gcc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

BUILD_DIR := build
OBJECT_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/picemu

SOURCES := src/main.c src/hex_loader.c src/pic10f200.c \
	src/disassembler.c src/vcd_writer.c
OBJECTS := $(patsubst src/%.c,$(OBJECT_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)

UNIT_TEST := $(BUILD_DIR)/tests/test_cpu
UNIT_TEST_OBJECTS := $(OBJECT_DIR)/test_cpu.o \
	$(OBJECT_DIR)/pic10f200.o $(OBJECT_DIR)/disassembler.o \
	$(OBJECT_DIR)/hex_loader.o

.PHONY: all clean firmware run test unit-test

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(OBJECT_DIR)/%.o: src/%.c | $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJECT_DIR)/test_cpu.o: test/unit/test_cpu.c | $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

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

-include $(DEPFILES) $(OBJECT_DIR)/test_cpu.d
