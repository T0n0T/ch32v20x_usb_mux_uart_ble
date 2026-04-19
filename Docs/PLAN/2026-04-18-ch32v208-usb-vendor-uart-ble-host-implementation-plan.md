# CH32V208 USB Vendor UART + BLE Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于当前仓库实现一个采用 `Makefile` 构建的 `CH32V208` 固件，使用单一 `USB Vendor Interface` 复用协议向 Host 提供 `4` 路由 Vendor 驱动注册的标准串口对象，并同时提供完整 `BLE Host 1 对 3` 控制能力。

**Architecture:** 固件以 `裸机 + TMOS + ISR 最小化` 为基础，新增 `App/usb_mux_dev` 应用层目录，拆分为 `usb/proto/uart/ble/common/config/net` 模块。USB 使用 `EP0 + EP2 IN/OUT + EP1 IN`，所有业务统一进入私有帧协议和逻辑通道模型，UART 与 BLE 独立管理，后续通过预留 `NET_MGMT/NET_DATA` 通道扩展以太网功能。

**Tech Stack:** `riscv-none-embed-gcc`、WCH `BLE/libwchble.a`、仓库内 `Peripheral` 驱动、手写 `Makefile`

---

## 文件结构

说明：以下目录和文件职责为实施规划视角的抽象描述；当前仓库的实际构建拆分已落地为 `Scripts/` 和 `Out/`，并在仓库根目录生成 `compile_commands.json` 供编辑器索引使用。

补充说明：本项目按 `CH32V208` 的实际可写 Flash 总量 `448KB`、其中 `128KB` 为快速 Flash、`32KB` 可配置区域可作 Flash 或 RAM、另有 `32KB` 固定 RAM 的约束推进实现。后续智能体在处理链接脚本、烧录布局、内存规划时，应优先遵循该约束，而不是仅依据工具日志中的容量输出做判断。

### 新建目录

- `App/usb_mux_dev/`
- `App/usb_mux_dev/include/`
- `App/usb_mux_dev/common/`
- `App/usb_mux_dev/config/`
- `App/usb_mux_dev/proto/`
- `App/usb_mux_dev/usb/`
- `App/usb_mux_dev/uart/`
- `App/usb_mux_dev/ble/`
- `App/usb_mux_dev/net/`
- `Scripts/`
- `Out/`
- `compile_commands.json`

### 计划中的核心文件职责

- `Makefile`
  - 顶层构建入口，定义目标、目录和默认规则
- `Scripts/toolchain.mk`
  - 工具链、公共编译参数、链接参数
- `Scripts/sources.mk`
  - 应用层和仓库内库源码列表
- `Scripts/rules.mk`
  - 通用编译和链接规则
- `compile_commands.json`
  - 自动生成的编译数据库，供 `clangd`、`clang-tidy` 和编辑器代码索引使用
- `App/usb_mux_dev/main.c`
  - 固件主入口，初始化顺序和主循环
- `App/usb_mux_dev/include/app_init.h`
  - 顶层初始化接口
- `App/usb_mux_dev/common/ring_buffer.[ch]`
  - 通用环形缓冲
- `App/usb_mux_dev/common/event_queue.[ch]`
  - 统一事件队列
- `App/usb_mux_dev/common/stats.[ch]`
  - 统计结构与累计接口
- `App/usb_mux_dev/config/board_caps.h`
  - 能力位和队列尺寸
- `App/usb_mux_dev/config/uart_map_config.h`
  - 4 路逻辑 UART 的编译期映射
- `App/usb_mux_dev/proto/vendor_proto.h`
  - 协议头、opcode、状态码、通道枚举
- `App/usb_mux_dev/proto/vendor_proto_codec.c`
  - 帧编解码和 CRC
- `App/usb_mux_dev/proto/vendor_router.c`
  - 协议分发
- `App/usb_mux_dev/usb/usb_dev_ll.c`
  - USB 设备低层初始化、中断和 EP 处理
- `App/usb_mux_dev/usb/usb_rx_fsm.c`
  - 下行组帧状态机
- `App/usb_mux_dev/usb/usb_tx_sched.c`
  - 上行调度器
- `App/usb_mux_dev/uart/uart_manager.c`
  - 4 路逻辑 UART 管理
- `App/usb_mux_dev/uart/uart_port_ch32v20x.c`
  - UART 外设和 GPIO 适配
- `App/usb_mux_dev/ble/ble_host_manager.c`
  - BLE 全局状态机
- `App/usb_mux_dev/ble/ble_link_fsm.c`
  - BLE 单槽位状态机
- `App/usb_mux_dev/ble/ble_att_cache.c`
  - 发现结果缓存
- `App/usb_mux_dev/net/net_mgr_stub.c`
  - 以太网预留桩模块
- `Docs/DESIGN/CH32V208_USB_VENDOR_UART_BLE_HOST_FIRMWARE_DESIGN.md`
  - 已批准设计文档，实施过程中如发现必须偏离设计，需要同步修订

## Task 1: 建立 Makefile 基线和应用骨架

**Files:**
- Create: `Makefile`
- Create: `mk/toolchain.mk`
- Create: `mk/sources.mk`
- Create: `mk/rules.mk`
- Create: `App/usb_mux_dev/main.c`
- Create: `App/usb_mux_dev/include/app_init.h`
- Create: `App/usb_mux_dev/app_init.c`
- Modify: `Ld/Link.ld`
- Modify: `Startup/startup_ch32v20x_D8W.S`

- [ ] **Step 1: 创建顶层 Makefile 和 mk 目录**

```make
TARGET := ch32v208_usb_mux_ble_host
OUT_DIR := Out

include Scripts/toolchain.mk
include Scripts/sources.mk
include Scripts/rules.mk

.PHONY: all clean size list compile_commands flash flash-sdi flash-openocd flatten-wch-lib

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
```

- [ ] **Step 2: 写入工具链和源码清单**

```make
# Scripts/toolchain.mk
CC := riscv-none-embed-gcc
AS := riscv-none-embed-gcc
OBJCOPY := riscv-none-embed-objcopy
OBJDUMP := riscv-none-embed-objdump
SIZE := riscv-none-embed-size

ARCH_FLAGS := -march=rv32imacxw -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore
CFLAGS := $(ARCH_FLAGS) -Os -g -ffunction-sections -fdata-sections -fno-common -Wall -Wextra -I.
LDFLAGS := $(ARCH_FLAGS) -T Ld/Link.ld -nostartfiles -Wl,--gc-sections -Wl,-Map,$(OUT_DIR)/$(TARGET).map --specs=nano.specs --specs=nosys.specs
LIBS := BLE/libwchble.a
```

```make
# Scripts/sources.mk
APP_SRCS := \
	App/usb_mux_dev/main.c \
	App/usb_mux_dev/app_init.c

CORE_SRCS := \
	Core/core_riscv.c \
	Debug/debug.c \
	User/system_ch32v20x.c

PERIPH_SRCS := \
	Peripheral/src/ch32v20x_gpio.c \
	Peripheral/src/ch32v20x_rcc.c \
	Peripheral/src/ch32v20x_usart.c \
	Peripheral/src/ch32v20x_misc.c
```

- [ ] **Step 3: 写入最小应用入口**

```c
// App/usb_mux_dev/main.c
#include "debug.h"
#include "app_init.h"
#include "CONFIG.h"
#include "HAL.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    APP_Init();
    while(1)
    {
        TMOS_SystemProcess();
    }
}
```

```c
// App/usb_mux_dev/app_init.c
#include "app_init.h"

void APP_Init(void)
{
}
```

- [ ] **Step 4: 调整链接脚本和启动文件以指向新入口**

```ld
/* Ld/Link.ld */
ENTRY(Reset_Handler)
```

```asm
/* Startup/startup_ch32v20x_D8W.S */
    .extern main
```

- [ ] **Step 5: 构建验证**

Run: `make clean && make all`

Expected:
- 成功生成 `compile_commands.json`
- 成功生成 `Out/ch32v208_usb_mux_ble_host.elf`
- 成功生成 `Out/ch32v208_usb_mux_ble_host.hex`
- 成功生成 `Out/ch32v208_usb_mux_ble_host.lst`
- 成功生成 `Out/ch32v208_usb_mux_ble_host.map`
- 执行 `make flash` 时可通过仓库内 `Scripts/WCH/CommunicationLib/libmcuupdate.so` 完成烧录、校验与复位
- 如需同时打开 `SDI print`，可执行 `make flash-sdi`
- 如需复现或继续定位 OpenOCD 识别问题，可执行 `make flash-openocd`；若镜像地址触及 `0x08028000` 以上，该命令应直接失败并提示当前 `wch_riscv` 驱动能力不足
- 如需将仓库内 `CommunicationLib` 压平成单层普通文件目录，可执行 `make flatten-wch-lib`

- [ ] **Step 6: MCU 启动验证**

操作：
- 下载固件
- 打开调试串口日志

预期：
- MCU 正常启动，不复位死循环
- 若已保留最小打印，应出现启动日志

失败观察点：
- 链接错误
- 启动后无串口日志
- 进入 HardFault 或中断异常

- [ ] **Step 7: Commit**

```bash
git add Makefile mk App/usb_mux_dev Ld/Link.ld Startup/startup_ch32v20x_D8W.S
git commit -m "build: add makefile baseline and app skeleton"
```

## Task 2: 建立公共基础设施和协议头

**Files:**
- Create: `App/usb_mux_dev/common/ring_buffer.h`
- Create: `App/usb_mux_dev/common/ring_buffer.c`
- Create: `App/usb_mux_dev/common/event_queue.h`
- Create: `App/usb_mux_dev/common/event_queue.c`
- Create: `App/usb_mux_dev/common/stats.h`
- Create: `App/usb_mux_dev/common/stats.c`
- Create: `App/usb_mux_dev/config/board_caps.h`
- Create: `App/usb_mux_dev/config/uart_map_config.h`
- Create: `App/usb_mux_dev/proto/vendor_proto.h`
- Create: `App/usb_mux_dev/proto/vendor_proto_codec.c`
- Create: `App/usb_mux_dev/proto/vendor_proto_codec.h`
- Modify: `mk/sources.mk`

- [ ] **Step 1: 写入能力、通道和状态码定义**

```c
// App/usb_mux_dev/proto/vendor_proto.h
typedef enum {
    VP_CH_SYS       = 0x00,
    VP_CH_UART_CTRL = 0x01,
    VP_CH_UART_DATA = 0x02,
    VP_CH_BLE_MGMT  = 0x10,
    VP_CH_BLE_CONN  = 0x11,
    VP_CH_NET_MGMT  = 0x20,
    VP_CH_NET_DATA  = 0x21,
} vp_channel_type_t;

typedef enum {
    VP_MSG_CMD  = 0x01,
    VP_MSG_RSP  = 0x02,
    VP_MSG_EVT  = 0x03,
    VP_MSG_DATA = 0x04,
} vp_msg_type_t;

#define VP_MAGIC   0xA55A
#define VP_VERSION 0x01
```

- [ ] **Step 2: 写入帧头、提示包和协议辅助结构**

```c
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  header_len;
    uint16_t total_len;
    uint16_t seq;
    uint16_t ref_seq;
    uint8_t  ch_type;
    uint8_t  ch_id;
    uint8_t  msg_type;
    uint8_t  opcode;
    uint16_t flags;
    uint16_t status;
    uint16_t payload_len;
    uint16_t header_crc16;
    uint16_t reserved;
} vp_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  urgent_flags;
    uint16_t pending_bitmap;
    uint16_t dropped_bitmap;
    uint16_t reserved;
} vp_irq_hint_t;
```

- [ ] **Step 3: 实现最小 CRC、编解码和公共缓冲**

```c
// App/usb_mux_dev/proto/vendor_proto_codec.c
uint16_t VP_Crc16(const uint8_t *data, uint16_t len);
int VP_EncodeHeader(vp_hdr_t *hdr);
int VP_DecodeHeader(const uint8_t *buf, uint16_t len, vp_hdr_t *hdr);
int VP_CheckFrameBounds(const vp_hdr_t *hdr, uint16_t max_len);
```

```c
// App/usb_mux_dev/common/ring_buffer.h
typedef struct {
    uint8_t  *buf;
    uint16_t  size;
    volatile uint16_t head;
    volatile uint16_t tail;
} ring_buffer_t;
```

- [ ] **Step 4: 写入编译期 UART 映射和能力表**

```c
// App/usb_mux_dev/config/uart_map_config.h
#define UART_LOGIC_PORT_COUNT 4

typedef struct {
    uint8_t enable;
    uint8_t phy_uart_id;
    uint32_t tx_port;
    uint16_t tx_pin;
    uint32_t rx_port;
    uint16_t rx_pin;
} uart_map_cfg_t;

extern const uart_map_cfg_t g_uart_map_cfg[UART_LOGIC_PORT_COUNT];
```

```c
// App/usb_mux_dev/config/board_caps.h
#define APP_USB_MAX_FRAME_LEN     512
#define APP_UART_PORT_COUNT       4
#define APP_BLE_MAX_LINKS         3
#define APP_CAP_NET_RESERVED      1
```

- [ ] **Step 5: 更新 Makefile 源码清单**

```make
APP_SRCS += \
	App/usb_mux_dev/common/ring_buffer.c \
	App/usb_mux_dev/common/event_queue.c \
	App/usb_mux_dev/common/stats.c \
	App/usb_mux_dev/proto/vendor_proto_codec.c
```

- [ ] **Step 6: 构建验证**

Run: `make clean && make all`

Expected:
- 新增公共模块成功编译
- 不出现重复定义和头文件包含错误

- [ ] **Step 7: 日志验证**

操作：
- 下载固件
- 确认启动日志仍然正常

预期：
- 与 Task 1 相比没有新增异常复位

- [ ] **Step 8: Commit**

```bash
git add App/usb_mux_dev/common App/usb_mux_dev/config App/usb_mux_dev/proto mk/sources.mk
git commit -m "core: add shared buffers and vendor protocol definitions"
```

## Task 3: 实现 USB 低层、收发状态机和 SYS 通道

**Files:**
- Create: `App/usb_mux_dev/usb/usb_dev_ll.h`
- Create: `App/usb_mux_dev/usb/usb_dev_ll.c`
- Create: `App/usb_mux_dev/usb/usb_rx_fsm.h`
- Create: `App/usb_mux_dev/usb/usb_rx_fsm.c`
- Create: `App/usb_mux_dev/usb/usb_tx_sched.h`
- Create: `App/usb_mux_dev/usb/usb_tx_sched.c`
- Create: `App/usb_mux_dev/proto/vendor_router.h`
- Create: `App/usb_mux_dev/proto/vendor_router.c`
- Modify: `App/usb_mux_dev/app_init.c`
- Modify: `App/usb_mux_dev/main.c`
- Modify: `mk/sources.mk`

- [ ] **Step 1: 从参考例程迁移 USB 设备端点骨架**

```c
// App/usb_mux_dev/usb/usb_dev_ll.h
void USBMUX_DeviceInit(void);
void USBMUX_Poll(void);
void USBMUX_NotifyHint(uint16_t pending_bitmap, uint16_t dropped_bitmap, uint8_t urgent_flags);
```

```c
// App/usb_mux_dev/usb/usb_dev_ll.c
void USBHD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
static void USBMUX_HandleEp2Out(uint16_t rx_len);
static void USBMUX_HandleEp2InDone(void);
static void USBMUX_HandleEp1InDone(void);
```

- [ ] **Step 2: 实现下行组帧状态机**

```c
// App/usb_mux_dev/usb/usb_rx_fsm.c
typedef enum {
    USB_RX_IDLE = 0,
    USB_RX_HEADER,
    USB_RX_PAYLOAD,
    USB_RX_DISPATCH,
    USB_RX_DROP,
} usb_rx_state_t;

void USBRX_Init(void);
void USBRX_PushBytes(const uint8_t *data, uint16_t len);
void USBRX_Process(void);
```

- [ ] **Step 3: 实现上行调度器和最小路由**

```c
// App/usb_mux_dev/usb/usb_tx_sched.c
void USBTX_Init(void);
void USBTX_Process(void);
int USBTX_QueueRsp(const vp_hdr_t *hdr, const uint8_t *payload);
int USBTX_QueueEvt(const vp_hdr_t *hdr, const uint8_t *payload);
int USBTX_QueueData(const vp_hdr_t *hdr, const uint8_t *payload);
```

```c
// App/usb_mux_dev/proto/vendor_router.c
int VPRouter_Dispatch(const vp_hdr_t *hdr, const uint8_t *payload)
{
    switch(hdr->ch_type)
    {
        case VP_CH_SYS:
            return VPSys_Handle(hdr, payload);
        default:
            return VP_STATUS_ERR_UNSUPPORTED_CHANNEL;
    }
}
```

- [ ] **Step 4: 先实现 SYS 最小命令**

```c
static int VPSys_Handle(const vp_hdr_t *hdr, const uint8_t *payload)
{
    switch(hdr->opcode)
    {
        case VP_SYS_GET_DEV_INFO:
        case VP_SYS_GET_CAPS:
        case VP_SYS_HEARTBEAT:
            return VPSys_HandleBasic(hdr, payload);
        default:
            return VP_STATUS_ERR_UNSUPPORTED_OPCODE;
    }
}
```

- [ ] **Step 5: 在初始化路径挂接 USB**

```c
// App/usb_mux_dev/app_init.c
void APP_Init(void)
{
    WCHBLE_Init();
    HAL_Init();
    USBRX_Init();
    USBTX_Init();
    USBMUX_DeviceInit();
}
```

```c
// App/usb_mux_dev/main.c
while(1)
{
    USBRX_Process();
    USBTX_Process();
    TMOS_SystemProcess();
}
```

- [ ] **Step 6: 构建验证**

Run: `make clean && make all`

Expected:
- USB 相关源文件成功编译并链接
- 不出现未解析的中断符号

- [ ] **Step 7: 现场验证点**

操作：
- 将固件烧录到板卡
- 连接 USB 到 Host
- 观察 Host 是否识别到 Vendor 设备

预期：
- 设备可枚举
- `GET_DEV_INFO`、`GET_CAPS`、`HEARTBEAT` 可通过 Host 侧工具或自定义帧发送后得到响应
- MCU 日志可观察到 USB 初始化和 EP2 收发路径

若当前环境无法直接构造协议帧：
- 停下并通知用户需要使用最小 Host 调试工具发送三条 SYS 命令
- 要求用户回传设备枚举结果和三条命令的响应内容

- [ ] **Step 8: Commit**

```bash
git add App/usb_mux_dev/usb App/usb_mux_dev/proto/vendor_router.* App/usb_mux_dev/app_init.c App/usb_mux_dev/main.c mk/sources.mk
git commit -m "usb: add vendor transport skeleton and sys channel"
```

## Task 4: 实现 UART 管理与 4 路逻辑串口通道

**Files:**
- Create: `App/usb_mux_dev/uart/uart_manager.h`
- Create: `App/usb_mux_dev/uart/uart_manager.c`
- Create: `App/usb_mux_dev/uart/uart_port_ch32v20x.h`
- Create: `App/usb_mux_dev/uart/uart_port_ch32v20x.c`
- Modify: `App/usb_mux_dev/proto/vendor_proto.h`
- Modify: `App/usb_mux_dev/proto/vendor_router.c`
- Modify: `App/usb_mux_dev/main.c`
- Modify: `mk/sources.mk`

- [ ] **Step 1: 定义 UART 管理上下文和状态机**

```c
typedef enum {
    UART_DISABLED = 0,
    UART_CLOSED,
    UART_OPENING,
    UART_OPEN,
    UART_DRAINING,
    UART_ERROR,
} uart_port_state_t;

typedef struct {
    uint8_t logic_port;
    uint8_t phy_uart_id;
    uint8_t state;
    ring_buffer_t rx_rb;
    ring_buffer_t tx_rb;
    uint32_t baudrate;
    uint32_t stats_drop_bytes;
} uart_port_ctx_t;
```

- [ ] **Step 2: 实现 CH32V20x UART 硬件适配**

```c
void UARTPort_ApplyMap(const uart_map_cfg_t *cfg);
void UARTPort_Open(uint8_t logic_port, uint32_t baudrate);
void UARTPort_Close(uint8_t logic_port);
void UARTPort_IrqRxByte(uint8_t logic_port, uint8_t data);
int  UARTPort_TryKickTx(uint8_t logic_port);
```

- [ ] **Step 3: 实现 UART 控制命令**

```c
int VPUartCtrl_Handle(const vp_hdr_t *hdr, const uint8_t *payload)
{
    switch(hdr->opcode)
    {
        case VP_UART_GET_PORT_CAP:
        case VP_UART_GET_PORT_MAP:
        case VP_UART_OPEN:
        case VP_UART_CLOSE:
        case VP_UART_SET_LINE_CODING:
        case VP_UART_GET_STATS:
            return UartMgr_HandleCtrl(hdr, payload);
        default:
            return VP_STATUS_ERR_UNSUPPORTED_OPCODE;
    }
}
```

- [ ] **Step 4: 实现 UART_DATA 上下行通道**

```c
int VPUartData_Handle(const vp_hdr_t *hdr, const uint8_t *payload)
{
    return UartMgr_WriteFromHost(hdr->ch_id, payload, hdr->payload_len);
}

void UartMgr_PumpRxToUsb(void)
{
    for(uint8_t port = 0; port < APP_UART_PORT_COUNT; ++port)
    {
        UartMgr_FlushOnePortRx(port);
    }
}
```

- [ ] **Step 5: 将 UART 处理挂接到主循环和路由器**

```c
// App/usb_mux_dev/main.c
while(1)
{
    USBRX_Process();
    UartMgr_Process();
    USBTX_Process();
    TMOS_SystemProcess();
}
```

```c
// App/usb_mux_dev/proto/vendor_router.c
case VP_CH_UART_CTRL:
    return VPUartCtrl_Handle(hdr, payload);
case VP_CH_UART_DATA:
    return VPUartData_Handle(hdr, payload);
```

- [ ] **Step 6: 构建验证**

Run: `make clean && make all`

Expected:
- UART 管理模块成功编译
- 所有已启用 UART IRQ 符号都已正确链接

- [ ] **Step 7: 现场验证点**

操作：
- 在硬件上接入已知串口回环或外部 UART 设备
- Host 侧打开 4 路串口中的一到两路
- 发送已知字节序列

预期：
- `GET_PORT_MAP` 返回与编译配置一致
- 打开的逻辑端口能收发数据
- 未打开端口写入返回明确错误
- 若人为灌入超出缓冲数据，日志或统计中出现溢出计数

若当前环境无法连接真实 UART：
- 停下并通知用户验证 `GET_PORT_MAP`、`OPEN/CLOSE`、单端口收发和溢出统计四项
- 要求用户回传 Host 侧现象和 MCU 日志

- [ ] **Step 8: Commit**

```bash
git add App/usb_mux_dev/uart App/usb_mux_dev/proto/vendor_proto.h App/usb_mux_dev/proto/vendor_router.c App/usb_mux_dev/main.c mk/sources.mk
git commit -m "uart: add 4-port logical uart manager"
```

## Task 5: 实现 BLE 全局管理器与扫描/连接控制面

**Files:**
- Create: `App/usb_mux_dev/ble/ble_host_manager.h`
- Create: `App/usb_mux_dev/ble/ble_host_manager.c`
- Create: `App/usb_mux_dev/ble/ble_att_cache.h`
- Create: `App/usb_mux_dev/ble/ble_att_cache.c`
- Modify: `App/usb_mux_dev/app_init.c`
- Modify: `App/usb_mux_dev/proto/vendor_proto.h`
- Modify: `App/usb_mux_dev/proto/vendor_router.c`
- Modify: `mk/sources.mk`

- [ ] **Step 1: 从参考例程拆出 BLE 全局状态机**

```c
typedef enum {
    BLE_G_IDLE = 0,
    BLE_G_READY,
    BLE_G_SCANNING,
    BLE_G_CONNECTING,
    BLE_G_MIXED,
    BLE_G_STOPPING,
} ble_global_state_t;

typedef struct {
    uint8_t task_id;
    uint8_t state;
    uint8_t pending_connect_slot;
    uint8_t active_scan;
} ble_host_mgr_t;
```

- [ ] **Step 2: 建立 BLE 管理命令入口**

```c
int VPBleMgmt_Handle(const vp_hdr_t *hdr, const uint8_t *payload)
{
    switch(hdr->opcode)
    {
        case VP_BLE_GET_CAP:
        case VP_BLE_SET_SCAN_PARAM:
        case VP_BLE_SCAN_START:
        case VP_BLE_SCAN_STOP:
        case VP_BLE_CONNECT:
        case VP_BLE_DISCONNECT:
        case VP_BLE_GET_CONN_STATE:
            return BleHostMgr_HandleMgmt(hdr, payload);
        default:
            return VP_STATUS_ERR_UNSUPPORTED_OPCODE;
    }
}
```

- [ ] **Step 3: 适配 GAP Central 初始化和事件回调**

```c
void BleHostMgr_Init(void)
{
    GAPRole_CentralInit();
    GATT_InitClient();
    GATT_RegisterForInd(g_ble_mgr.task_id);
}

static void BleHostMgr_EventCb(gapRoleEvent_t *evt)
{
    switch(evt->gap.opcode)
    {
        case GAP_DEVICE_DISCOVERY_EVENT:
        case GAP_LINK_ESTABLISHED_EVENT:
        case GAP_LINK_TERMINATED_EVENT:
            BleHostMgr_HandleGapEvent(evt);
            break;
    }
}
```

- [ ] **Step 4: 建立扫描结果和连接状态上报**

```c
static void BleHostMgr_ReportScanResult(const gapDevRec_t *rec);
static void BleHostMgr_ReportConnState(uint8_t slot, uint16_t conn_handle, uint8_t state, uint16_t status);
```

- [ ] **Step 5: 将 BLE 管理挂接到初始化和路由器**

```c
// App/usb_mux_dev/app_init.c
void APP_Init(void)
{
    WCHBLE_Init();
    HAL_Init();
    BleHostMgr_Init();
    USBRX_Init();
    USBTX_Init();
    USBMUX_DeviceInit();
}
```

```c
// App/usb_mux_dev/proto/vendor_router.c
case VP_CH_BLE_MGMT:
    return VPBleMgmt_Handle(hdr, payload);
```

- [ ] **Step 6: 构建验证**

Run: `make clean && make all`

Expected:
- BLE 管理器成功编译
- `libwchble.a` 正常链接

- [ ] **Step 7: 现场验证点**

操作：
- 下载固件
- 用 Host 发送 `SET_SCAN_PARAM`、`SCAN_START`、`SCAN_STOP`
- 用真实 BLE 环境发送一次 `CONNECT`

预期：
- MCU 日志能看到扫描启动和停止
- 扫描时 `BLE_CONN` 或 `BLE_MGMT` 事件通道有扫描结果上报
- 连接成功或失败均有明确事件和状态码

若现场缺少 BLE 从设备：
- 停下并通知用户至少验证扫描结果是否正常上报
- 要求用户回传扫描日志和扫描到的设备列表

- [ ] **Step 8: Commit**

```bash
git add App/usb_mux_dev/ble/ble_host_manager.* App/usb_mux_dev/ble/ble_att_cache.* App/usb_mux_dev/app_init.c App/usb_mux_dev/proto/vendor_proto.h App/usb_mux_dev/proto/vendor_router.c mk/sources.mk
git commit -m "ble: add host manager for scan and connect control"
```

## Task 6: 实现 BLE 单槽位发现、读写和订阅状态机

**Files:**
- Create: `App/usb_mux_dev/ble/ble_link_fsm.h`
- Create: `App/usb_mux_dev/ble/ble_link_fsm.c`
- Modify: `App/usb_mux_dev/ble/ble_host_manager.c`
- Modify: `App/usb_mux_dev/proto/vendor_proto.h`
- Modify: `App/usb_mux_dev/proto/vendor_router.c`
- Modify: `mk/sources.mk`

- [ ] **Step 1: 定义单槽位状态机和上下文**

```c
typedef enum {
    BLE_L_IDLE = 0,
    BLE_L_CONNECT_PENDING,
    BLE_L_CONNECTING,
    BLE_L_CONNECTED,
    BLE_L_MTU_EXCHANGING,
    BLE_L_DISC_SERVICE,
    BLE_L_DISC_CHAR,
    BLE_L_DISC_DESC,
    BLE_L_READY,
    BLE_L_READING,
    BLE_L_WRITING_REQ,
    BLE_L_SUBSCRIBING,
    BLE_L_UNSUBSCRIBING,
    BLE_L_DISCONNECTING,
    BLE_L_ERROR,
} ble_link_state_t;

typedef struct {
    uint8_t slot_id;
    uint8_t state;
    uint16_t conn_handle;
    uint16_t mtu;
    uint8_t proc_busy;
} ble_link_ctx_t;
```

- [ ] **Step 2: 实现发现流程和 GATT procedure 仲裁**

```c
int BleLink_StartDiscoverServices(uint8_t slot);
int BleLink_StartDiscoverChars(uint8_t slot, uint16_t start_hdl, uint16_t end_hdl);
int BleLink_StartDiscoverDescs(uint8_t slot, uint16_t start_hdl, uint16_t end_hdl);
```

```c
static int BleLink_RequireIdleProc(ble_link_ctx_t *ctx)
{
    if(ctx->proc_busy)
    {
        return VP_STATUS_ERR_BUSY;
    }
    ctx->proc_busy = 1;
    return VP_STATUS_OK;
}
```

- [ ] **Step 3: 实现读写、订阅和通知上报**

```c
int BleLink_Read(uint8_t slot, uint16_t attr_handle);
int BleLink_WriteReq(uint8_t slot, uint16_t attr_handle, const uint8_t *buf, uint16_t len);
int BleLink_WriteCmd(uint8_t slot, uint16_t attr_handle, const uint8_t *buf, uint16_t len);
int BleLink_Subscribe(uint8_t slot, uint16_t cccd_handle);
int BleLink_Unsubscribe(uint8_t slot, uint16_t cccd_handle);
```

```c
static void BleLink_ReportNotify(uint8_t slot, uint16_t value_handle, const uint8_t *buf, uint16_t len);
```

- [ ] **Step 4: 将完整 BLE 命令接入路由器**

```c
// App/usb_mux_dev/proto/vendor_router.c
case VP_BLE_DISCOVER_SERVICES:
case VP_BLE_DISCOVER_CHARACTERISTICS:
case VP_BLE_DISCOVER_DESCRIPTORS:
case VP_BLE_READ:
case VP_BLE_WRITE_REQ:
case VP_BLE_WRITE_CMD:
case VP_BLE_SUBSCRIBE:
case VP_BLE_UNSUBSCRIBE:
case VP_BLE_EXCHANGE_MTU:
case VP_BLE_READ_RSSI:
case VP_BLE_UPDATE_CONN_PARAM:
    return VPBleMgmt_Handle(hdr, payload);
```

- [ ] **Step 5: 处理 GATT 回调并驱动状态迁移**

```c
static void BleHostMgr_ProcessGattMsg(gattMsgEvent_t *msg)
{
    uint8_t slot = BleHostMgr_FindSlotByConnHandle(msg->connHandle);
    BleLink_HandleGattMsg(slot, msg);
}
```

- [ ] **Step 6: 构建验证**

Run: `make clean && make all`

Expected:
- BLE 发现、读写和订阅模块成功编译
- 不出现 GATT 消息处理重复定义

- [ ] **Step 7: 现场验证点**

操作：
- 使用真实 BLE 从设备完成一次完整流程：
  1. 扫描
  2. 建连
  3. 服务发现
  4. 特征发现
  5. 读
  6. 写
  7. 订阅通知

预期：
- 每条命令均收到一个 `RSP`
- 长操作有后续 `EVT`
- 发现结果、读写结果和通知负载能回到 Host
- 连接槽位 busy 时并发命令返回 `ERR_BUSY`

若当前节点需要用户确认：
- 明确告知用户当前进入“BLE 完整闭环验证”阶段
- 要求用户按 7 个动作逐条执行
- 回传对应 MCU 日志和 Host 侧结果

- [ ] **Step 8: Commit**

```bash
git add App/usb_mux_dev/ble/ble_link_fsm.* App/usb_mux_dev/ble/ble_host_manager.c App/usb_mux_dev/proto/vendor_proto.h App/usb_mux_dev/proto/vendor_router.c mk/sources.mk
git commit -m "ble: add discovery read write and subscribe fsm"
```

## Task 7: 集成调度、错误处理、背压和 NET 预留

**Files:**
- Create: `App/usb_mux_dev/net/net_mgr_stub.h`
- Create: `App/usb_mux_dev/net/net_mgr_stub.c`
- Modify: `App/usb_mux_dev/usb/usb_tx_sched.c`
- Modify: `App/usb_mux_dev/usb/usb_rx_fsm.c`
- Modify: `App/usb_mux_dev/common/stats.[ch]`
- Modify: `App/usb_mux_dev/common/event_queue.[ch]`
- Modify: `App/usb_mux_dev/proto/vendor_router.c`
- Modify: `App/usb_mux_dev/config/board_caps.h`
- Modify: `mk/sources.mk`

- [ ] **Step 1: 为上行调度器加入优先级和配额**

```c
typedef enum {
    TXQ_RSP = 0,
    TXQ_EVT_HI,
    TXQ_EVT_LO,
    TXQ_DATA,
} usb_tx_queue_id_t;

void USBTX_Process(void)
{
    USBTX_PumpQueue(TXQ_RSP, 4);
    USBTX_PumpQueue(TXQ_EVT_HI, 2);
    USBTX_PumpQueue(TXQ_EVT_LO, 2);
    USBTX_PumpQueue(TXQ_DATA, 4);
}
```

- [ ] **Step 2: 加入坏帧、超长帧和 CRC 失败处理**

```c
if(hdr->total_len > APP_USB_MAX_FRAME_LEN)
{
    Stats_IncBadLen();
    USBMUX_NotifyHint(0, 0, APP_HINT_FLAG_PROTO_ERR);
    state = USB_RX_DROP;
}
```

- [ ] **Step 3: 统一统计和丢包事件**

```c
typedef struct {
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t crc_err;
    uint32_t overflow;
    uint32_t drop_bytes;
} stats_chan_t;
```

- [ ] **Step 4: 加入 NET 桩模块和预留路由**

```c
// App/usb_mux_dev/net/net_mgr_stub.c
int VPNet_Handle(const vp_hdr_t *hdr, const uint8_t *payload)
{
    (void)hdr;
    (void)payload;
    return VP_STATUS_ERR_UNSUPPORTED_OPCODE;
}
```

```c
// App/usb_mux_dev/proto/vendor_router.c
case VP_CH_NET_MGMT:
case VP_CH_NET_DATA:
    return VPNet_Handle(hdr, payload);
```

- [ ] **Step 5: 构建验证**

Run: `make clean && make all`

Expected:
- 所有集成模块编译通过
- `NET` 预留路径可编译但未启用真实业务

- [ ] **Step 6: 现场验证点**

操作：
- 构造错误帧、超长帧、未知通道、未知 opcode
- 在 UART 或 BLE 过程中制造缓冲压力

预期：
- Host 能收到明确错误响应或提示
- 统计计数增长
- MCU 不因坏帧或流量压力卡死

- [ ] **Step 7: Commit**

```bash
git add App/usb_mux_dev/net App/usb_mux_dev/usb App/usb_mux_dev/common App/usb_mux_dev/proto/vendor_router.c App/usb_mux_dev/config/board_caps.h mk/sources.mk
git commit -m "core: add scheduler backpressure and net stubs"
```

## Task 8: 文档收口、日志清理和最终验证清单

**Files:**
- Modify: `Docs/DESIGN/CH32V208_USB_VENDOR_UART_BLE_HOST_FIRMWARE_DESIGN.md`
- Create: `Docs/PLAN/validation-checklist-ch32v208-usb-uart-ble-host.md`
- Modify: `App/usb_mux_dev/proto/vendor_proto.h`
- Modify: `App/usb_mux_dev/main.c`
- Modify: `App/usb_mux_dev/ble/ble_host_manager.c`
- Modify: `App/usb_mux_dev/uart/uart_manager.c`

- [ ] **Step 1: 将实现结果回填到设计文档**

```md
- 最终端点号与设计一致
- UART 映射默认值已实现
- BLE 命令和事件编码已冻结
- NET 通道当前仅为预留
```

- [ ] **Step 2: 提炼单独验证清单文档**

```md
# CH32V208 USB UART BLE Host 验证清单

- 构建验证：`make clean && make all`
- USB 枚举验证：Vendor 设备可枚举
- SYS 通道验证：`GET_DEV_INFO / GET_CAPS / HEARTBEAT`
- UART 验证：4 路映射、打开关闭、收发、溢出
- BLE 验证：扫描、建连、发现、读写、订阅、通知
- 错误路径验证：坏帧、未知 opcode、busy、timeout
```

- [ ] **Step 3: 清理日志并统一关键日志格式**

```c
printf("[USB] EP2 OUT len=%u\r\n", len);
printf("[UART%u] open baud=%lu\r\n", port, baudrate);
printf("[BLE] slot=%u state=%u status=0x%04x\r\n", slot, state, status);
```

- [ ] **Step 4: 最终构建验证**

Run: `make clean && make all && make size`

Expected:
- 所有产物生成成功
- `size` 输出正常
- 无新的编译告警或仅保留可解释告警

- [ ] **Step 5: 最终现场验证停点**

向用户明确说明需要最终确认的内容：

- Host 侧是否出现预期的 4 个串口对象
- 串口对象是否与配置的物理 UART 一致
- BLE 1 对 3 是否能稳定建连
- 服务发现、读写和通知是否符合目标外设

要求用户回传：

- Host 枚举现象
- MCU 日志
- 关键命令响应
- BLE 目标设备实际行为

- [ ] **Step 6: Commit**

```bash
git add Docs/DESIGN/CH32V208_USB_VENDOR_UART_BLE_HOST_FIRMWARE_DESIGN.md Docs/PLAN/validation-checklist-ch32v208-usb-uart-ble-host.md App/usb_mux_dev
git commit -m "docs: finalize validation checklist and implementation notes"
```

## Self-Review

### Spec coverage

- 设计中的 `Makefile` 要求由 Task 1 覆盖
- 单一 `Vendor Interface + EP0/EP1/EP2` 由 Task 3 覆盖
- `4` 路 UART 编译期映射和 Host 可见串口语义由 Task 4 覆盖
- BLE `1 对 3` 扫描、建连、发现、读写、订阅由 Task 5 和 Task 6 覆盖
- 背压、错误码、统计和 NET 预留由 Task 7 覆盖
- 日志和人工停点验证约束由 Task 8 覆盖

### Placeholder scan

- 计划中未使用 `TBD`、`TODO`、`后续补充` 一类占位词
- 每个任务均给出了明确文件路径、命令、代码片段和验证项
- 验证项按用户要求使用“清单 + 日志/现象”，未要求落盘测试代码

### Type consistency

- 协议通道常量统一为 `VP_CH_*`
- 消息类型统一为 `VP_MSG_*`
- UART 状态统一为 `UART_*`
- BLE 全局状态统一为 `BLE_G_*`
- BLE 槽位状态统一为 `BLE_L_*`

Plan complete and saved to `Docs/PLAN/2026-04-18-ch32v208-usb-vendor-uart-ble-host-implementation-plan.md`. Two execution options:

1. Subagent-Driven (recommended) - I dispatch a fresh subagent per task, review between tasks, fast iteration

2. Inline Execution - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
