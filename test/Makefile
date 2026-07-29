XC8 ?= xc8-cc
PIC_DEVICE ?= 10F200
XC8FLAGS ?= -O2

BUILD_DIR := build
SOURCE := blink.c
ELF := $(BUILD_DIR)/blink.elf
HEX := $(BUILD_DIR)/blink.hex
STAMP := $(BUILD_DIR)/.firmware.stamp

# 可以在命令行传入：
#   make DFP=/path/to/PIC10-12Fxxx_DFP/version/xc8
# 或预先设置：
#   export PIC10_DFP=/path/to/.../xc8
DFP ?= $(PIC10_DFP)

# 如果没有显式设置，就尝试寻找前面文档推荐的用户级安装位置。
ifeq ($(strip $(DFP)),)
DFP := $(lastword $(sort $(wildcard \
	$(HOME)/.mchp_packs/Microchip/PIC10-12Fxxx_DFP/*/xc8)))
endif

# 根 Makefile 会传入绝对路径；直接在 test 目录运行时使用默认路径。
PICEMU ?= ../build/picemu

.PHONY: all firmware test run clean help

all: firmware

firmware: $(STAMP)

$(STAMP): $(SOURCE) Makefile | $(BUILD_DIR)
	@command -v "$(XC8)" >/dev/null 2>&1 || { \
		echo "错误：找不到 XC8 编译器：$(XC8)"; \
		echo "请安装 XC8，并把 xc8-cc 加入 PATH。"; \
		exit 1; \
	}
	@test -n "$(DFP)" || { \
		echo "错误：没有找到 PIC10-12Fxxx_DFP。"; \
		echo "请设置 PIC10_DFP，或者执行："; \
		echo "  make DFP=/path/to/PIC10-12Fxxx_DFP/version/xc8"; \
		exit 1; \
	}
	@test -d "$(DFP)" || { \
		echo "错误：DFP 路径不存在：$(DFP)"; \
		exit 1; \
	}
	$(XC8) -mcpu=$(PIC_DEVICE) -mdfp="$(DFP)" $(XC8FLAGS) \
		-o $(ELF) $(SOURCE)
	@test -f "$(HEX)" || { \
		echo "错误：XC8 编译完成，但没有生成 $(HEX)"; \
		exit 1; \
	}
	@touch $@

$(BUILD_DIR):
	mkdir -p $@

run: firmware
	@test -x "$(PICEMU)" || { \
		echo "错误：找不到模拟器 $(PICEMU)，请先在项目根目录执行 make"; \
		exit 1; \
	}
	$(PICEMU) $(HEX) --cycles 3500000

test: firmware
	@test -x "$(PICEMU)" || { \
		echo "错误：找不到模拟器 $(PICEMU)，请先在项目根目录执行 make"; \
		exit 1; \
	}
	sh test_blink.sh "$(PICEMU)" "$(HEX)"

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "可用目标："
	@echo "  make firmware  使用 XC8 编译 blink.c"
	@echo "  make run       使用模拟器运行 blink.hex"
	@echo "  make test      自动验证 GP0 闪烁行为"
	@echo "  make clean     删除 test/build"
