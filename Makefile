TARGET := ch32v208_usb_mux_ble_host
OUT_DIR := Out

include Scripts/sources.mk
include Scripts/toolchain.mk
include Scripts/rules.mk

.PHONY: all clean size list compile_commands flash flash-sdi flash-openocd flatten-wch-lib debug-openocd debug-gdb

all: compile_commands.json $(OUT_DIR)/$(TARGET).elf $(OUT_DIR)/$(TARGET).hex $(OUT_DIR)/$(TARGET).lst

compile_commands: compile_commands.json

clean:
	rm -rf $(OUT_DIR) compile_commands.json

size: $(OUT_DIR)/$(TARGET).elf
	$(SIZE) --format=berkeley $<

list: $(OUT_DIR)/$(TARGET).elf
	$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $(OUT_DIR)/$(TARGET).lst

flash: $(OUT_DIR)/$(TARGET).hex
	$(PYTHON) "$(WCH_FLASH_SCRIPT)" --file "$<" --chip "$(WCH_FLASH_CHIP)" --iface "$(WCH_FLASH_IFACE)" --speed "$(WCH_FLASH_SPEED)" --ops "$(WCH_FLASH_OPS)" --address "$(WCH_FLASH_ADDR)" --comm-lib-dir "$(WCH_COMM_LIB_DIR)"

flash-sdi: WCH_FLASH_SDI_PRINT=1
flash-sdi: flash

flash-openocd: $(OUT_DIR)/$(TARGET).openocd.hex
	$(PYTHON) "$(OPENOCD_FLASH_WRAPPER)" --image "$<" --openocd "$(OPENOCD)" --config "$(OPENOCD_CFG)" --command "program $(OPENOCD_FLASH_IMAGE) verify reset exit"

flatten-wch-lib:
	$(PYTHON) Scripts/flatten_wch_comm_lib.py "$(WCH_COMM_LIB_DIR)"

debug-openocd:
	$(OPENOCD) -f "$(OPENOCD_CFG)"

debug-gdb: $(OUT_DIR)/$(TARGET).elf
	$(GDB) -q "$<" \
		-ex "set architecture riscv:rv32" \
		-ex "target remote $(GDB_REMOTE)" \
		-ex "monitor reset halt" \
		-ex "thbreak main"
