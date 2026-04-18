# CH32V208 USB Vendor UART + BLE Host 固件设计

## 1. 文档目的

本文档用于定义基于本仓库为 `CH32V208` 设计的新固件方案。固件目标如下：

- 通过 `USB Device` 的 `Vendor Interface` 向 Host 提供 `4` 路标准串口能力
- 在同一固件中提供 `BLE Host` 主机能力
- BLE 首版即支持完整的扫描、连接、服务发现、特征发现、描述符发现、读写、订阅通知
- BLE 连接模型首版采用参考例程方式，先实现 `1 主对 3 从`
- 不采用通用 USB 设备类方案，不实现 CDC/复合标准类设备，直接采用私有协议
- USB 侧通过状态机和端点多路复用实现 UART、BLE 和系统管理通道
- 设计中预留后续扩展以太网功能的能力和协议位置
- 构建系统正式采用 `Makefile`

本文档不包含 Host 侧驱动或应用实现，但协议和交互行为必须足够完整，使 Host 团队可独立对接。

## 2. 参考基础

本设计基于仓库中以下内容抽象和扩展：

- `Docs/EXAM/BLE/BLE_USB`
  - 提供 `USB Device Vendor` 风格设备端点组织和 BLE 与 USB 混合工程基线
- `Docs/EXAM/BLE/MultiCentral`
  - 提供 `BLE Central 1 对 3` 连接模型和 TMOS 事件驱动范式
- `BLE/libwchble.a`
  - 闭源 BLE 库
- `USB/USB-Driver`
  - 官方 USB 抽象库
- `Docs/EXAM/ETH/NetLib`
  - 后续以太网扩展的接口预留参考

## 3. 需求边界

### 3.1 功能目标

- Host 看到 `4` 个标准串口
- 这 `4` 路串口直接映射到 MCU 的 UART 外设
- UART 与 BLE 无直接绑定关系
- UART 映射关系采用编译时可配置方式
- 设备 USB 描述符层面维持单一 `Vendor Interface`，由 Host Vendor 驱动在驱动层向操作系统注册 `4` 个标准串口对象，并同时承载 BLE Host 控制协议和系统协议
- BLE 首版即需要完整控制能力，而不是只做固定地址自动连接

### 3.2 不做内容

- 不做 CDC ACM、WinUSB 通用枚举兼容性方案设计
- 不做 Host 驱动和 Host 应用
- 不在本阶段实现以太网功能
- 不引入新的 RTOS

### 3.3 扩展要求

- 协议必须可向后兼容扩展
- 架构必须允许后续引入 `ETH_MGMT` 和 `ETH_DATA` 逻辑通道
- 代码边界必须足够清晰，避免 USB、UART、BLE、后续 ETH 相互耦合

## 4. 总体架构

固件采用单一 `USB Vendor Interface` 承载全部私有管理协议。Host 侧由 Vendor 驱动基于该单一接口向操作系统注册 `4` 路标准串口对象，同时复用同一接口完成 BLE Host 和系统管理协议交互。

整体上分为三层：

- `USB 传输层`
  - 负责枚举、端点收发、中断处理、数据搬运
- `协议复用层`
  - 负责帧格式、逻辑通道、多路复用、命令分发、响应和事件上报
- `业务执行层`
  - 负责 UART 管理、BLE Host 管理、系统管理和未来的网络管理

### 4.1 总线模型

- 一个 `Vendor Interface`
- 一个 `Bulk OUT` 端点承载所有 Host -> Device 命令和数据
- 一个 `Bulk IN` 端点承载所有 Device -> Host 的响应、事件和数据
- 一个 `Interrupt IN` 端点承载轻量异步提示

### 4.2 功能平面

- `UART 平面`
  - 4 路逻辑串口，映射到编译期配置的物理 UART
- `BLE Host 平面`
  - 提供扫描、建连、发现、读写、订阅和通知接收能力
- `System 平面`
  - 提供版本、能力、统计、日志级别、复位等控制
- `Future Net 平面`
  - 预留给后续以太网功能

## 5. USB 设备模型

### 5.1 端点布局

建议固定如下：

- `EP0`
  - 标准控制端点
- `EP2 OUT (0x02)`
  - Host -> Device `Bulk OUT`
- `EP2 IN (0x82)`
  - Device -> Host `Bulk IN`
- `EP1 IN (0x81)`
  - Device -> Host `Interrupt IN`

说明：

- `EP2 OUT/IN` 组成统一数据和命令通道
- `EP1 IN` 仅发送短提示，不发送完整业务负载
- 这样能最大限度复用 `BLE_USB` 示例的设备端点风格，降低首版风险

### 5.2 USB 角色约束

- 不使用标准 CDC 类描述符
- 不使用多类复合设备方案
- 所有业务都挂在 Vendor 语义之下
- 设备侧 USB 描述符只维护单一 Vendor Interface
- Host 侧如需注册为标准串口，由其 Vendor 驱动自行向操作系统暴露串口对象

## 6. 私有协议设计

## 6.1 传输规则

- 协议运行在 `EP2 OUT/EP2 IN` 上
- 协议以帧为单位，不以 USB `64B` 包边界作为消息边界
- 支持跨多个 USB 包组帧
- USB 链路可靠性由控制器保证，协议层不实现重传
- 协议层实现 `magic`、版本、长度、帧头 CRC、负载 CRC 校验

## 6.2 统一帧头

所有私有帧统一使用如下固定头：

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
```

字段定义：

- `magic`
  - 固定值 `0xA55A`
- `version`
  - 协议版本，首版固定 `0x01`
- `header_len`
  - 首版固定 `24`
- `total_len`
  - 整帧长度
- `seq`
  - 当前发送方单方向递增帧序号
- `ref_seq`
  - 响应帧或异步结果关联原命令
- `ch_type`
  - 逻辑通道类型
- `ch_id`
  - 逻辑通道编号或槽位编号
- `msg_type`
  - `CMD/RSP/EVT/DATA`
- `opcode`
  - 命令或事件编码
- `flags`
  - 标志位
- `status`
  - 状态码，命令帧固定为 `0`
- `payload_len`
  - 负载长度
- `header_crc16`
  - 帧头 CRC16
- `reserved`
  - 预留

### 6.3 消息类型

- `0x01 CMD`
- `0x02 RSP`
- `0x03 EVT`
- `0x04 DATA`

要求：

- 每个 `CMD` 必须返回且只返回一个 `RSP`
- 长操作的异步结果通过 `EVT` 上报
- UART 原始字节流和 BLE 通知负载通过 `DATA` 上报

### 6.4 逻辑通道分配

- `0x00 SYS`
  - `ch_id = 0`
- `0x01 UART_CTRL`
  - `ch_id = 0..3`
- `0x02 UART_DATA`
  - `ch_id = 0..3`
- `0x10 BLE_MGMT`
  - `ch_id = 0`
- `0x11 BLE_CONN`
  - `ch_id = 0..2`
- `0x20 NET_MGMT`
  - 预留
- `0x21 NET_DATA`
  - 预留

### 6.5 状态码

统一状态码建议如下：

- `0x0000 OK`
- `0x0001 ERR_BAD_MAGIC`
- `0x0002 ERR_BAD_VERSION`
- `0x0003 ERR_BAD_LEN`
- `0x0004 ERR_BAD_HDR_CRC`
- `0x0005 ERR_BAD_PAYLOAD_CRC`
- `0x0006 ERR_UNSUPPORTED_CHANNEL`
- `0x0007 ERR_UNSUPPORTED_OPCODE`
- `0x0008 ERR_INVALID_PARAM`
- `0x0009 ERR_INVALID_STATE`
- `0x000A ERR_BUSY`
- `0x000B ERR_NO_RESOURCE`
- `0x000C ERR_TIMEOUT`
- `0x000D ERR_OVERFLOW`
- `0x0010 ERR_UART_MAP_INVALID`
- `0x0011 ERR_UART_NOT_OPEN`
- `0x0020 ERR_BLE_SLOT_INVALID`
- `0x0021 ERR_BLE_NOT_CONNECTED`
- `0x0022 ERR_BLE_DISC_NOT_READY`
- `0x0023 ERR_BLE_ATT`
- `0x0030 ERR_INTERNAL`

### 6.6 Interrupt 提示包

`EP1 IN` 不发送完整业务帧，只发送轻量异步提示包，建议固定为 `8B`：

```c
typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  urgent_flags;
    uint16_t pending_bitmap;
    uint16_t dropped_bitmap;
    uint16_t reserved;
} vp_irq_hint_t;
```

字段语义：

- `version`
  - 首版固定 `0x01`
- `urgent_flags`
  - 标识断链、严重错误、缓冲溢出等高优先级事件
- `pending_bitmap`
  - 指示哪些逻辑队列中存在待从 `EP2 IN` 读取的完整帧
- `dropped_bitmap`
  - 指示哪些逻辑队列发生过数据丢弃

Host 收到提示后，应继续从 `EP2 IN` 读取完整 `RSP/EVT/DATA` 帧。

## 7. UART 设计

## 7.1 UART 功能语义

- Host 侧有 `4` 路逻辑串口
- 每一路逻辑串口对应一个 MCU UART 外设
- 逻辑串口与物理 UART 的绑定由编译期配置
- 运行时可查询当前映射和能力
- UART 与 BLE 无任何默认映射关系

## 7.2 编译期映射

采用配置表方式而不是大量 `#ifdef`：

- 每个逻辑端口定义：
  - 是否启用
  - 物理 UART 实例号
  - TX 引脚
  - RX 引脚
  - RTS/CTS 是否启用
  - 默认串口参数

推荐配置文件：

- `App/usb_mux_dev/config/uart_map_config.h`

## 7.3 UART 控制命令

`ch_type = UART_CTRL, ch_id = 0..3`

至少支持：

- `GET_PORT_CAP`
- `GET_PORT_MAP`
- `OPEN`
- `CLOSE`
- `SET_LINE_CODING`
- `SET_FLOW_CTRL`
- `SET_MODEM_SIGNALS`
- `GET_MODEM_STATUS`
- `SEND_BREAK`
- `FLUSH_RX`
- `FLUSH_TX`
- `GET_STATS`

## 7.4 UART 数据通道

`ch_type = UART_DATA, ch_id = 0..3`

- Host -> Device
  - 负载为要发送到物理 UART 的原始字节
- Device -> Host
  - 负载为 MCU UART 接收到的原始字节

不额外包裹二级子头，减少协议开销。

## 7.5 UART 状态机

每个逻辑 UART 采用如下状态：

- `UART_DISABLED`
- `UART_CLOSED`
- `UART_OPENING`
- `UART_OPEN`
- `UART_DRAINING`
- `UART_ERROR`

## 7.6 UART 并发与缓冲

- 首版采用 `中断 + RingBuffer`
- ISR 中只做寄存器读写和缓冲搬运
- 协议打包和上报在前台任务中完成
- 如果上行队列满，允许丢弃数据，但必须：
  - 记录统计
  - 发送溢出事件

## 8. BLE Host 设计

## 8.1 BLE 能力范围

首版 BLE Host 必须支持：

- 设置扫描参数
- 开始扫描
- 停止扫描
- 按地址连接
- 断开连接
- 服务发现
- 特征发现
- 描述符发现
- 特征读
- 写请求
- 写命令
- 订阅通知
- 取消订阅
- MTU 交换
- RSSI 读取
- 连接参数更新
- 查询连接状态

## 8.2 连接模型

- 最多 `3` 条从连接
- 全局最多同时进行 `1` 个建连流程
- 每条连接使用一个固定槽位 `0..2`
- 首版保持与 `MultiCentral` 示例一致的 `1 主对 3 从` 基线

## 8.3 BLE 全局状态机

- `BLE_G_IDLE`
- `BLE_G_READY`
- `BLE_G_SCANNING`
- `BLE_G_CONNECTING`
- `BLE_G_MIXED`
- `BLE_G_STOPPING`

全局状态机负责：

- 扫描参数
- 建连仲裁
- 空闲槽位管理
- 待执行命令队列
- 去重处理

## 8.4 BLE 单连接槽位状态机

每个连接槽位采用如下细粒度状态：

- `BLE_L_IDLE`
- `BLE_L_CONNECT_PENDING`
- `BLE_L_CONNECTING`
- `BLE_L_CONNECTED`
- `BLE_L_MTU_EXCHANGING`
- `BLE_L_DISC_SERVICE`
- `BLE_L_DISC_CHAR`
- `BLE_L_DISC_DESC`
- `BLE_L_READY`
- `BLE_L_READING`
- `BLE_L_WRITING_REQ`
- `BLE_L_SUBSCRIBING`
- `BLE_L_UNSUBSCRIBING`
- `BLE_L_DISCONNECTING`
- `BLE_L_ERROR`

要求：

- 每个连接槽位同一时刻只允许一个 `GATT procedure`
- 若槽位忙，则新命令直接返回 `ERR_BUSY`
- Host 通过显式命令驱动发现流程

## 8.5 BLE 管理命令

`ch_type = BLE_MGMT, ch_id = 0`

至少定义：

- `GET_BLE_CAP`
- `SET_SCAN_PARAM`
- `SCAN_START`
- `SCAN_STOP`
- `CONNECT`
- `DISCONNECT`
- `DISCOVER_SERVICES`
- `DISCOVER_CHARACTERISTICS`
- `DISCOVER_DESCRIPTORS`
- `READ`
- `WRITE_REQ`
- `WRITE_CMD`
- `SUBSCRIBE`
- `UNSUBSCRIBE`
- `EXCHANGE_MTU`
- `READ_RSSI`
- `UPDATE_CONN_PARAM`
- `GET_CONN_STATE`

## 8.6 BLE 连接事件通道

`ch_type = BLE_CONN, ch_id = 0..2`

上报内容包括：

- `EVT_CONN_STATE_CHANGED`
- `EVT_SCAN_REPORT`
- `EVT_SERVICE_FOUND`
- `EVT_CHAR_FOUND`
- `EVT_DESC_FOUND`
- `EVT_READ_RESULT`
- `EVT_WRITE_RESULT`
- `EVT_NOTIFY_DATA`
- `EVT_INDICATE_DATA`
- `EVT_RSSI_RESULT`

## 9. System 管理设计

`ch_type = SYS, ch_id = 0`

首版建议固定支持：

- `GET_DEV_INFO`
- `GET_CAPS`
- `GET_STATS`
- `CLEAR_STATS`
- `SET_LOG_LEVEL`
- `GET_LOG_LEVEL`
- `HEARTBEAT`
- `SOFT_RESET`

### 9.1 能力位

能力位至少应覆盖：

- UART 路数
- BLE 最大连接数
- BLE 是否支持扫描
- BLE 是否支持发现
- BLE 是否支持通知订阅
- NET 预留能力位

## 10. 并发模型与调度

## 10.1 总体执行模型

首版固件采用：

- 裸机主循环
- `TMOS_SystemProcess()` 作为 BLE 事件主循环
- USB 中断驱动
- UART 中断驱动
- 所有业务逻辑在前台状态机中推进

不引入新的 RTOS。

## 10.2 设计原则

- ISR 只搬运，不决策
- 协议解析在前台完成
- USB、UART、BLE 不直接跨层耦合，统一通过协议帧和事件队列交互

## 10.3 关键执行单元

- `usb_rx_fsm`
- `usb_tx_sched`
- `uart_port_fsm[4]`
- `ble_global_fsm`
- `ble_link_fsm[3]`
- `event_agg`
- `buffer_pool`

## 10.4 USB 接收状态机

- `USB_RX_IDLE`
- `USB_RX_HEADER`
- `USB_RX_PAYLOAD`
- `USB_RX_DISPATCH`
- `USB_RX_DROP`

规则：

- 错帧直接丢弃
- 不能阻塞后续帧
- 命令队列满则返回 `ERR_BUSY` 或 `ERR_NO_RESOURCE`

## 10.5 USB 发送优先级

建议发送优先级：

1. `RSP`
2. 高优先级 `EVT`
3. BLE 控制结果
4. `UART_DATA`
5. 低优先级统计和日志

目的是避免 UART 大流量饿死 BLE 控制面。

## 10.6 背压和丢包策略

### 控制命令

- 不允许静默丢弃
- 必须返回成功或明确失败

### UART 数据

- 上行队列满时允许受控丢弃
- 必须：
  - 增加 drop 计数
  - 上报 `UART_OVERRUN EVT`

### BLE 数据

- 上行队列满时允许受控丢弃
- 必须：
  - 增加 drop 计数
  - 上报 `BLE_DATA_DROP EVT`

## 10.7 定时器与超时

统一定义以下超时：

- USB 半帧接收超时
- BLE 建连超时
- BLE 发现超时
- BLE 读写超时
- UART drain 超时
- 上行聚合超时

## 11. 代码组织

建议新增应用目录：

- `App/usb_mux_dev/`
- `App/usb_mux_dev/include/`
- `App/usb_mux_dev/usb/`
- `App/usb_mux_dev/proto/`
- `App/usb_mux_dev/uart/`
- `App/usb_mux_dev/ble/`
- `App/usb_mux_dev/common/`
- `App/usb_mux_dev/config/`
- `App/usb_mux_dev/net/`

### 11.1 推荐核心文件

- `App/usb_mux_dev/main.c`
- `App/usb_mux_dev/include/app_init.h`
- `App/usb_mux_dev/usb/usb_dev_ll.c`
- `App/usb_mux_dev/usb/usb_rx_fsm.c`
- `App/usb_mux_dev/usb/usb_tx_sched.c`
- `App/usb_mux_dev/proto/vendor_proto.h`
- `App/usb_mux_dev/proto/vendor_proto_codec.c`
- `App/usb_mux_dev/proto/vendor_router.c`
- `App/usb_mux_dev/uart/uart_manager.c`
- `App/usb_mux_dev/uart/uart_port_ch32v20x.c`
- `App/usb_mux_dev/ble/ble_host_manager.c`
- `App/usb_mux_dev/ble/ble_link_fsm.c`
- `App/usb_mux_dev/ble/ble_att_cache.c`
- `App/usb_mux_dev/common/ring_buffer.c`
- `App/usb_mux_dev/common/event_queue.c`
- `App/usb_mux_dev/common/stats.c`
- `App/usb_mux_dev/config/board_caps.h`
- `App/usb_mux_dev/config/uart_map_config.h`
- `App/usb_mux_dev/net/net_mgr_stub.c`

### 11.2 关键数据结构

建议使用统一上下文对象：

```c
typedef struct {
    usb_dev_ctx_t     usb;
    proto_ctx_t       proto;
    uart_mgr_t        uart_mgr;
    ble_host_mgr_t    ble_mgr;
    sys_mgr_t         sys_mgr;
    net_mgr_stub_t    net_mgr;
    event_queue_t     evt_hi;
    event_queue_t     evt_lo;
    frame_queue_t     rsp_q;
    frame_queue_t     data_q;
    stats_global_t    stats;
} app_ctx_t;
```

UART 端口上下文：

```c
typedef struct {
    uint8_t            logic_port;
    uint8_t            phy_uart_id;
    uint8_t            state;
    uart_line_coding_t line;
    uart_flow_ctrl_t   flow;
    ring_buffer_t      rx_rb;
    ring_buffer_t      tx_rb;
    uint16_t           rx_high_wm;
    uint16_t           tx_high_wm;
    uint32_t           flags;
    uart_stats_t       stats;
} uart_port_ctx_t;
```

BLE 槽位上下文：

```c
typedef struct {
    uint8_t             slot_id;
    uint8_t             state;
    uint8_t             addr_type;
    uint8_t             peer_addr[6];
    uint16_t            conn_handle;
    uint16_t            mtu;
    uint16_t            pending_opcode;
    uint8_t             proc_busy;
    ble_service_cache_t svc_cache;
    ble_char_cache_t    char_cache;
    ble_desc_cache_t    desc_cache;
    ble_sub_table_t     sub_table;
    ble_link_stats_t    stats;
} ble_link_ctx_t;
```

## 12. Makefile 构建系统设计

## 12.1 基本原则

- 正式构建入口必须是 `Makefile`
- `.wvproj` 或 MRS 自动生成工程仅作为辅助，不作为事实来源
- 所有源码、头文件路径、库路径、链接脚本、输出产物均由 `Makefile` 显式声明
- 输出目录与源码目录分离

## 12.2 推荐目录

- `Makefile`
- `mk/toolchain.mk`
- `mk/sources.mk`
- `mk/ble.mk`
- `mk/usb.mk`
- `mk/periph.mk`
- `mk/rules.mk`
- `build/`
- `out/`

## 12.3 推荐目标

- `make`
- `make all`
- `make clean`
- `make size`
- `make list`
- `make flash`

## 12.4 构建要求

- 不依赖 IDE 自动生成 `sources.mk`
- 源文件列表手工维护
- 自动生成 `.d` 依赖文件
- 链接脚本使用仓库内固定路径
- 使用统一工具链配置文件管理编译参数

## 12.5 构建变量建议

- `TARGET := ch32v208_usb_mux_ble_host`
- `BUILD_DIR := build`
- `OUT_DIR := out`
- `LINKER_SCRIPT := Ld/Link.ld`
- `BLE_LIB := BLE/libwchble.a`

## 13. 以太网扩展预留

本阶段不实现 ETH 功能，但必须预留：

- `NET_MGMT`
- `NET_DATA`
- 能力位
- 路由注册接口
- `net_mgr_stub`

扩展原则：

- 后续新增 ETH 功能时，不修改 USB 总线模型
- 优先通过新增逻辑通道和 opcode 扩展
- 不改变现有固定帧头

## 14. 文档交付要求

本项目至少应输出以下文档：

- 固件总体设计说明
- USB Vendor 协议说明
- UART 虚拟串口能力说明
- BLE Host 控制协议说明
- 错误码与异常恢复说明
- 以太网扩展预留说明

每条命令文档需固定包含：

- 命令名称
- 通道信息
- 请求负载
- 响应负载
- 异步事件
- 状态码
- 前置条件
- 状态迁移
- 超时和失败处理

## 15. 实施阶段划分

建议分五阶段实施：

### 阶段 1：Makefile 基线

- 建立独立构建系统
- 编译通过最小主工程
- 验证 BLE 闭源库和 USB 官方库链接

### 阶段 2：USB 协议骨架

- 建立 Vendor Interface
- 打通 `SYS` 通道
- 完成基础帧收发

### 阶段 3：UART 功能

- 实现 4 路逻辑串口
- 完成 UART 控制和数据通道
- 完成统计和溢出事件

### 阶段 4：BLE Host 完整功能

- 重构 `MultiCentral` 为模块化 BLE Host 管理器
- 完成扫描、建连、发现、读写、订阅和通知上报

### 阶段 5：稳定性与文档收口

- 完成超时、背压、统计一致性
- 固化协议文档
- 校验 ETH 预留兼容性

## 16. 验证策略

### 16.1 构建验证

- `make clean && make all`
- 生成 `.elf/.hex/.lst/.map`

### 16.2 协议验证

- 错误 `magic`
- 错误版本
- 错误长度
- 错误 CRC
- 未知通道
- 未知 opcode
- busy 冲突

### 16.3 UART 验证

- 4 路独立收发
- 不同串口参数切换
- 背压与溢出
- 统计准确性

### 16.4 BLE 验证

- 扫描
- 1 对 3 建连
- 服务发现
- 特征发现
- 描述符发现
- 读写
- 订阅和通知
- 断链恢复

## 17. 版本兼容策略

- 协议版本首版为 `0x01`
- 新能力通过 `GET_CAPS` 协商
- 新功能优先追加 opcode 和 payload 字段
- 固定帧头不轻易修改
- Host 必须忽略未知能力位
- 固件必须对未知旧 Host 请求返回明确错误码

## 18. 设计结论

本设计最终结论如下：

- 使用单一 `USB Vendor Interface`
- 使用 `EP0 + Bulk IN/OUT + Interrupt IN`
- 全部功能通过统一私有协议和逻辑通道复用
- `4` 路 UART 独立映射 MCU 物理串口
- BLE Host 首版即支持完整功能，连接模型为 `1 主对 3 从`
- 固件采用 `裸机 + TMOS + ISR 最小化`
- 后续以太网通过 `NET_MGMT/NET_DATA` 平滑扩展
- 正式构建系统采用 `Makefile`

该设计可作为后续实施计划和代码实现的唯一架构依据。
