TARGET := ch32v208_usb_mux_ble_host
OUT_DIR := Out

include Scripts/sources.mk
include Scripts/toolchain.mk
include Scripts/rules.mk

.PHONY: all clean size list compile_commands flash

all: compile_commands.json $(OUT_DIR)/$(TARGET).elf $(OUT_DIR)/$(TARGET).hex $(OUT_DIR)/$(TARGET).lst

compile_commands: compile_commands.json

clean:
	rm -rf $(OUT_DIR) compile_commands.json

size: $(OUT_DIR)/$(TARGET).elf
	$(SIZE) --format=berkeley $<

list: $(OUT_DIR)/$(TARGET).elf
	$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $(OUT_DIR)/$(TARGET).lst

flash: $(OUT_DIR)/$(TARGET).elf
	$(OPENOCD) -f "$(OPENOCD_CFG)" $(OPENOCD_FLASH_CMDS)
