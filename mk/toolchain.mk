TOOLCHAIN_BIN ?= /usr/share/MRS2/MRS-linux-x64/resources/app/resources/linux/components/WCH/Toolchain/RISC-V\ Embedded\ GCC15/bin
TOOLCHAIN_PREFIX ?= $(TOOLCHAIN_BIN)/riscv32-wch-elf-

ifeq ($(origin CC), default)
CC := $(TOOLCHAIN_PREFIX)gcc
endif
ifeq ($(origin CC), undefined)
CC := $(TOOLCHAIN_PREFIX)gcc
endif

ifeq ($(origin AS), default)
AS := $(TOOLCHAIN_PREFIX)gcc
endif
ifeq ($(origin AS), undefined)
AS := $(TOOLCHAIN_PREFIX)gcc
endif

ifeq ($(origin OBJCOPY), default)
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
endif
ifeq ($(origin OBJCOPY), undefined)
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
endif

ifeq ($(origin OBJDUMP), default)
OBJDUMP := $(TOOLCHAIN_PREFIX)objdump
endif
ifeq ($(origin OBJDUMP), undefined)
OBJDUMP := $(TOOLCHAIN_PREFIX)objdump
endif

ifeq ($(origin SIZE), default)
SIZE := $(TOOLCHAIN_PREFIX)size
endif
ifeq ($(origin SIZE), undefined)
SIZE := $(TOOLCHAIN_PREFIX)size
endif

ARCH_FLAGS := -march=rv32imacxw -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore
INCLUDES := \
	-I. \
	-IApp/usb_mux_dev/common \
	-IApp/usb_mux_dev/config \
	-IApp/usb_mux_dev/include \
	-IApp/usb_mux_dev/proto \
	-IApp/usb_mux_dev/uart \
	-IApp/usb_mux_dev/usb \
	-IBLE \
	-ICore \
	-IDebug \
	-IPeripheral/inc \
	-IUser

CFLAGS := $(ARCH_FLAGS) -Os -g -ffunction-sections -fdata-sections -fno-common -Wall -Wextra $(INCLUDES)
ASFLAGS := $(ARCH_FLAGS) -x assembler-with-cpp $(INCLUDES)
LDFLAGS := $(ARCH_FLAGS) -T Ld/Link.ld -nostartfiles -Wl,--gc-sections -Wl,--print-memory-usage -Wl,-Map,$(OUT_DIR)/$(TARGET).map --specs=nano.specs --specs=nosys.specs
LIBS := BLE/libwchble.a
