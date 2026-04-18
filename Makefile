TARGET := ch32v208_usb_mux_ble_host
BUILD_DIR := build
OUT_DIR := out

include mk/toolchain.mk
include mk/sources.mk
include mk/rules.mk

.PHONY: all clean size list

all: $(OUT_DIR)/$(TARGET).elf $(OUT_DIR)/$(TARGET).hex $(OUT_DIR)/$(TARGET).lst

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)

size: $(OUT_DIR)/$(TARGET).elf
	$(SIZE) --format=berkeley $<

list: $(OUT_DIR)/$(TARGET).elf
	$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $(OUT_DIR)/$(TARGET).lst
