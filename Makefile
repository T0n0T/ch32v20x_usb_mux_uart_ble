TARGET := ch32v208_usb_mux_ble_host
OUT_DIR := Out

include Scripts/sources.mk
include Scripts/toolchain.mk
include Scripts/rules.mk

.PHONY: all clean size list

all: $(OUT_DIR)/$(TARGET).elf $(OUT_DIR)/$(TARGET).hex $(OUT_DIR)/$(TARGET).lst

clean:
	rm -rf $(OUT_DIR)

size: $(OUT_DIR)/$(TARGET).elf
	$(SIZE) --format=berkeley $<

list: $(OUT_DIR)/$(TARGET).elf
	$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $(OUT_DIR)/$(TARGET).lst
