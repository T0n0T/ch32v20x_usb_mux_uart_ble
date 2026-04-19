TOOLCHAIN_BIN ?= /opt/riscv-wch-toolchain/bin
TOOLCHAIN_PREFIX ?= $(TOOLCHAIN_BIN)/riscv32-wch-elf-
PYTHON ?= python3
OPENOCD_BIN ?= /opt/openocd-wch/bin/
OPENOCD ?= $(OPENOCD_BIN)/openocd
OPENOCD_CFG ?= Scripts/openocd/ch32v208-wch-riscv.cfg
GDB ?= gdb-multiarch
GDB_REMOTE ?= :3333
OPENOCD_FLASH_IMAGE ?= $(OUT_DIR)/$(TARGET).openocd.hex
OPENOCD_FLASH_CMDS ?= -c "program $(OPENOCD_FLASH_IMAGE) verify reset exit"
OPENOCD_FLASH_WRAPPER ?= Scripts/openocd_flash.py
WCH_FLASH_SCRIPT ?= Scripts/wch_flash.py
WCH_COMM_LIB_DIR ?= Scripts/WCH/CommunicationLib
WCH_FLASH_CHIP ?= 5
WCH_FLASH_IFACE ?= 1
WCH_FLASH_SPEED ?= 3
WCH_FLASH_OPS_BASE ?= 15
WCH_FLASH_SDI_PRINT ?= 0
WCH_FLASH_OPS ?= $(if $(filter 1,$(WCH_FLASH_SDI_PRINT)),31,$(WCH_FLASH_OPS_BASE))
WCH_FLASH_ADDR ?= 0x08000000

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

CFLAGS := $(ARCH_FLAGS) -Os -g -ffunction-sections -fdata-sections -fno-common -Wall -Wextra $(INCLUDES)
ASFLAGS := $(ARCH_FLAGS) -x assembler-with-cpp $(INCLUDES)
LDFLAGS := $(ARCH_FLAGS) -T Ld/Link.ld -nostartfiles -Wl,--gc-sections -Wl,--print-memory-usage -Wl,-Map,$(OUT_DIR)/$(TARGET).map --specs=nano.specs --specs=nosys.specs
