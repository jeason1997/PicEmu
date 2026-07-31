# 调用者可通过环境变量或 make 参数覆盖编译器；未设置时从 PATH 查找。
# 不在项目中写死安装路径，以兼容不同 XC8 版本、用户目录和 CI 环境。
XC8 ?= xc8-cc
PIC_DEVICE ?= 10F200
XC8FLAGS ?= -O2

BUILD_DIR := build
SOURCE := main.c
ELF := $(BUILD_DIR)/firmware.elf
HEX := $(BUILD_DIR)/firmware.hex
STAMP := $(BUILD_DIR)/.firmware.stamp

DFP ?= $(PIC10_DFP)
ifeq ($(strip $(DFP)),)
DFP := $(lastword $(sort $(wildcard \
	$(HOME)/.mchp_packs/Microchip/PIC10-12Fxxx_DFP/*/xc8)))
endif

PICEMU ?= ../../build/picemu

.PHONY: all firmware run clean

all: firmware
firmware: $(STAMP)

$(STAMP): $(SOURCE) Makefile ../common.mk | $(BUILD_DIR)
	@command -v "$(XC8)" >/dev/null 2>&1 || { \
		echo "错误：找不到XC8编译器：$(XC8)"; exit 1; \
	}
	@test -n "$(DFP)" && test -d "$(DFP)" || { \
		echo "错误：没有找到PIC10-12Fxxx_DFP，请设置PIC10_DFP或DFP"; \
		exit 1; \
	}
	$(XC8) -mcpu=$(PIC_DEVICE) -mdfp="$(DFP)" $(XC8FLAGS) \
		-o $(ELF) $(SOURCE)
	@test -f "$(HEX)" || { echo "错误：没有生成$(HEX)"; exit 1; }
	@touch $@

$(BUILD_DIR):
	mkdir -p $@

run: firmware
	@test -x "$(PICEMU)" || { \
		echo "错误：找不到$(PICEMU)，请先在项目根目录执行make"; exit 1; \
	}
	$(PICEMU) $(HEX) $(SIM_ARGS)

clean:
	rm -rf $(BUILD_DIR)
