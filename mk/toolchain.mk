CC := riscv-none-embed-gcc
AS := riscv-none-embed-gcc
OBJCOPY := riscv-none-embed-objcopy
OBJDUMP := riscv-none-embed-objdump
SIZE := riscv-none-embed-size

ARCH_FLAGS := -march=rv32imacxw -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore
INCLUDES := \
	-I. \
	-IApp/usb_mux_dev/include \
	-IBLE \
	-ICore \
	-IDebug \
	-IPeripheral/inc \
	-IUser

CFLAGS := $(ARCH_FLAGS) -Os -g -ffunction-sections -fdata-sections -fno-common -Wall -Wextra $(INCLUDES)
ASFLAGS := $(ARCH_FLAGS) -x assembler-with-cpp $(INCLUDES)
LDFLAGS := $(ARCH_FLAGS) -T Ld/Link.ld -nostartfiles -Wl,--gc-sections -Wl,-Map,$(OUT_DIR)/$(TARGET).map --specs=nano.specs --specs=nosys.specs
LIBS := BLE/libwchble.a
