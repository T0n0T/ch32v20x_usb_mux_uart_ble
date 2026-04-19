# CH32V208 USB Vendor UART + BLE Host

本仓库实现面向 `CH32V208` 的固件工程，目标是在单一 `USB Vendor Interface` 上同时提供：

- `4` 路 UART 能力
- `BLE Host` 主机能力
- 基础系统管理能力

当前工程使用 `Makefile` 构建，默认通过仓库内的 `WCH CommunicationLib` 完成烧录。

## 1. 项目状态

当前仓库已包含：

- 基于 `Makefile` 的构建链路
- `USB Vendor` 设备端基础框架
- `4` 路逻辑 UART 管理
- `BLE Host 1 对 3` 基础管理框架
- 编译期可配置的运行灯
- `clangd` 参数兼容配置

当前仓库不包含：

- Host 侧驱动
- Host 侧上位机应用
- 以太网功能正式实现

## 2. 目标芯片与存储约定

本项目默认面向 `CH32V208`，并采用以下约定：

- 实际可写 Flash 总量按 `448KB` 理解
- 其中 `128KB` 为快速 Flash 区
- 另有 `32KB` 可配置区域，可配置为 Flash 或 RAM
- 另有 `32KB` 固定 RAM 区

涉及链接脚本、烧录布局、内存规划时，应优先遵循上述项目约定，不应仅依据调试或烧录工具的容量日志直接下结论。

## 3. 目录结构

主要目录如下：

- `App/usb_mux_dev`
  - 当前应用层代码
- `BLE`
  - WCH BLE 库与头文件
- `Core`
  - RISC-V 内核相关代码
- `Debug`
  - 调试串口、延时等基础支持
- `Docs`
  - 设计文档、计划文档、参考资料
- `Ld`
  - 链接脚本
- `Peripheral`
  - CH32V20x 外设库
- `Scripts`
  - 构建辅助、烧录脚本、OpenOCD 诊断脚本
- `Startup`
  - 启动文件
- `User`
  - 系统初始化、中断等基础文件

## 4. 构建环境

默认工具链配置见 [Scripts/toolchain.mk](/home/d/Documents/code/ch32v20x/Scripts/toolchain.mk)。

通常需要具备：

- `make`
- `python3`
- WCH RISC-V GCC 工具链
- 可选：`gdb-multiarch`
- 若需默认烧录：可用的 WCH 下载器与 `CommunicationLib`

默认工具链前缀为：

```text
/opt/riscv-wch-toolchain/bin/riscv32-wch-elf-
```

## 5. 常用命令

### 5.1 构建

```bash
make
```

生成物位于 `Out/`：

- `Out/ch32v208_usb_mux_ble_host.elf`
- `Out/ch32v208_usb_mux_ble_host.hex`
- `Out/ch32v208_usb_mux_ble_host.lst`
- `Out/ch32v208_usb_mux_ble_host.map`

### 5.2 清理

```bash
make clean
```

### 5.3 查看体积

```bash
make size
```

### 5.4 生成反汇编清单

```bash
make list
```

### 5.5 默认烧录

```bash
make flash
```

### 5.6 打开 SDI print 后烧录

```bash
make flash-sdi
```

### 5.7 OpenOCD 诊断烧录

```bash
make flash-openocd
```

注意：

- 该入口仅保留为诊断用途
- 若镜像触及 `0x08028000` 及以上地址，应直接视为当前 OpenOCD 驱动能力不足
- 不应把这类失败直接解释为本项目 `CH32V208` 容量约定错误

### 5.8 启动 OpenOCD / GDB 调试

```bash
make debug-openocd
make debug-gdb
```

## 6. 烧录说明

当前默认烧录链路为：

```text
make flash
  -> Scripts/wch_flash.py
  -> Scripts/WCH/CommunicationLib/libmcuupdate.so
```

其中：

- `make flash` 默认使用 `ops=15`
- `make flash-sdi` 使用 `ops=31`
- 当前项目中可按 `31 = 16 + 8 + 4 + 2 + 1` 理解，其中额外的 `16` 位用于打开 `SDI print`

详细说明见：

- [Docs/WCH_FLASH_SCRIPT.md](/home/d/Documents/code/ch32v20x/Docs/WCH_FLASH_SCRIPT.md)

## 7. 当前主要配置点

### 7.1 顶层固件配置

位于 [config.h](/home/d/Documents/code/ch32v20x/App/usb_mux_dev/include/config.h)。

当前包含：

- BLE 相关容量与开关
- 调试串口开关
- 运行灯配置

运行灯默认配置为：

- `PA0`
- 低电平点亮
- `500ms` 翻转一次

### 7.2 UART 映射配置

位于：

- [uart_map_config.h](/home/d/Documents/code/ch32v20x/App/usb_mux_dev/config/uart_map_config.h)
- [uart_map_config.c](/home/d/Documents/code/ch32v20x/App/usb_mux_dev/config/uart_map_config.c)

### 7.3 工具链与烧录参数

位于 [toolchain.mk](/home/d/Documents/code/ch32v20x/Scripts/toolchain.mk)。

## 8. 文档入口

建议先看以下文档：

- [README.md](/home/d/Documents/code/ch32v20x/README.md)
- [CH32V208_USB_VENDOR_UART_BLE_HOST_FIRMWARE_DESIGN.md](/home/d/Documents/code/ch32v20x/Docs/DESIGN/CH32V208_USB_VENDOR_UART_BLE_HOST_FIRMWARE_DESIGN.md)
- [WCH_FLASH_SCRIPT.md](/home/d/Documents/code/ch32v20x/Docs/WCH_FLASH_SCRIPT.md)

## 9. 已知事项

- `flash-openocd` 当前仅为诊断入口，不是默认烧录方案
- `clangd` 通过仓库根目录 `.clangd` 规避了 WCH GCC 私有参数兼容问题
- 若在虚拟机中烧录失败，应优先检查 WCH 下载器 USB 是否已正确直通到当前系统
