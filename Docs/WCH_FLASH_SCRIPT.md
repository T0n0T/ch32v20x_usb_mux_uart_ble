# `wch_flash.py` 烧录脚本说明

## 1. 目的

`Scripts/wch_flash.py` 用于在本仓库中通过 WCH 官方 `CommunicationLib` 动态库烧录 `CH32V208` 固件。

该脚本是当前项目默认烧录入口，对应 `make flash` 与 `make flash-sdi`。

## 2. 调用关系

默认调用链如下：

```text
make flash
  -> Scripts/wch_flash.py
  -> Scripts/WCH/CommunicationLib/libmcuupdate.so
  -> WCH-Link / WCH 调试下载器
  -> CH32V208
```

`make flash-sdi` 与 `make flash` 使用同一脚本，仅额外打开 `SDI print` 使能位。

对应参数差异为：

- `make flash`
  - `ops = 15`
- `make flash-sdi`
  - `ops = 31`

## 3. 默认参数

`Makefile` 中默认调用形式为：

```bash
python3 Scripts/wch_flash.py \
  --file Out/ch32v208_usb_mux_ble_host.hex \
  --chip 5 \
  --iface 1 \
  --speed 3 \
  --ops 15 \
  --address 0x08000000 \
  --comm-lib-dir Scripts/WCH/CommunicationLib
```

各参数含义如下：

- `--file`
  - 待烧录固件路径，通常为 `Out/*.hex`
- `--chip`
  - WCH 芯片类型编号
  - 当前项目 `CH32V208` 使用 `5`
- `--iface`
  - 调试接口模式
  - `0 = 1-wire`
  - `1 = 2-wire`
  - 当前项目默认使用 `1`
- `--speed`
  - 烧录速度
  - `1 = high`
  - `2 = middle`
  - `3 = low`
  - 当前项目默认使用 `3`
- `--ops`
  - 操作位图
  - `8 = erase`
  - `4 = program`
  - `2 = verify`
  - `1 = reset`
  - 默认 `15` 表示 `擦除 + 下载 + 校验 + 复位`
  - `make flash-sdi` 时使用 `31`
  - 当前项目中可按 `31 = 16 + 8 + 4 + 2 + 1` 理解，其中额外的 `16` 位用于打开 `SDI print`
- `--address`
  - 烧录基地址
  - 当前项目默认使用 `0x08000000` 的 Flash alias 地址
- `--comm-lib-dir`
  - `libmcuupdate.so` 与配套动态库所在目录

## 4. 动态库查找顺序

脚本会按以下顺序查找 `CommunicationLib`：

1. `--comm-lib-dir` 显式传入的目录
2. 仓库内默认目录 `Scripts/WCH/CommunicationLib`
3. MounRiver Studio 系统安装目录中的默认 `CommunicationLib`

目录中至少需要存在：

- `libmcuupdate.so`
- `libusb-1.0.so*`
- `libhidapi-libusb.so*`
- `libhidapi-hidraw.so*`
- `libjaylink.so*`

## 5. 当前项目约定

本项目关于烧录的关键约定如下：

- 默认使用 `make flash`
- 默认烧录地址为 `0x08000000`
- 当前项目按 `CH32V208` 的实际可写 Flash 总量 `448KB` 理解
- 其中 `128KB` 为快速 Flash
- 另有 `32KB` 可配置区域，可配置为 Flash 或 RAM
- 另有 `32KB` 固定 RAM

涉及地址、链接脚本、烧录布局时，应以本项目约定和芯片手册为准，不应仅依据工具日志中的容量输出直接下结论。

## 6. 与 OpenOCD 入口的区别

仓库还保留了 `make flash-openocd`，但它仅作为诊断入口，不是默认烧录方案。

原因是当前 `wch_riscv` 驱动仍可能把目标错误识别为：

- `flash size = 160kbytes`
- `ROM 128 kbytes`
- `RAM 64 kbytes`

并在镜像触及 `0x08028000` 及以上地址时报：

- `no flash bank found`

因此：

- `make flash`
  - 当前默认、优先使用
- `make flash-openocd`
  - 仅用于诊断，不应用于替代默认烧录链路

## 7. 常用命令

### 7.1 默认烧录

```bash
make flash
```

### 7.2 打开 SDI print 后烧录

```bash
make flash-sdi
```

说明：

- 该目标会把 `WCH_FLASH_SDI_PRINT` 置为 `1`
- 最终传给 `wch_flash.py` 的 `--ops` 会从 `15` 切换为 `31`

### 7.3 直接调用脚本

```bash
python3 Scripts/wch_flash.py \
  --file Out/ch32v208_usb_mux_ble_host.hex \
  --chip 5 \
  --iface 1 \
  --speed 3 \
  --ops 15 \
  --address 0x08000000 \
  --comm-lib-dir Scripts/WCH/CommunicationLib
```

## 8. 常见失败现象

### 8.1 `failed to load WCH communication library`

说明脚本没有找到可用的 `CommunicationLib`，或目录中的依赖动态库不完整。

优先检查：

- `Scripts/WCH/CommunicationLib/libmcuupdate.so` 是否存在
- 配套 `libusb`、`hidapi`、`jaylink` 动态库是否存在

### 8.2 `McuCompiler_SetTargetChip failed`

说明在设置目标芯片或接口模式阶段就失败，优先检查：

- `--chip` 是否正确
- `--iface` 是否与当前下载器连接方式匹配
- WCH 下载器是否已连接并被系统识别

### 8.3 `MRSFunc_FlashOperationExB failed with code XXX`

说明进入烧录阶段后由 `libmcuupdate.so` 返回错误。

当前仓库内未维护一份官方错误码表，因此不应直接凭数字含义下结论。

建议优先检查：

1. 主机是否枚举到 WCH 下载器
2. 若运行在虚拟机内，USB 设备是否已直通到虚拟机
3. 固件文件是否存在且为最新构建产物
4. 下载器与目标板供电、连线、复位状态是否正常

## 9. 相关文件

- `Makefile`
- `Scripts/wch_flash.py`
- `Scripts/toolchain.mk`
- `Scripts/flatten_wch_comm_lib.py`
- `Scripts/openocd_flash.py`
- `Scripts/openocd/ch32v208-wch-riscv.cfg`
