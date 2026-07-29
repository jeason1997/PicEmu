CC ?= gcc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

BUILD_DIR := build
OBJECT_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/picemu

SOURCES := src/main.c src/hex_loader.c src/pic10f200.c
OBJECTS := $(patsubst src/%.c,$(OBJECT_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all clean firmware run test

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(OBJECT_DIR)/%.o: src/%.c | $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(OBJECT_DIR):
	mkdir -p $@

# PIC 固件由 test/Makefile 管理，这里提供一个方便的转发入口。
firmware:
	$(MAKE) -C test firmware

run: all firmware
	$(TARGET) test/build/blink.hex --cycles 3500000

test: all
	$(MAKE) -C test test PICEMU="$(abspath $(TARGET))"

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C test clean

-include $(DEPFILES)
