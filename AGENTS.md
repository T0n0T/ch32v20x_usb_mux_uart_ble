# AGENTS.md

## 文档语言

- 后续项目文档默认使用中文，除非用户明确指定使用其他语言。
- 这里的“项目文档”包括但不限于设计文档、实施计划、协议说明、测试说明、README 以及其他说明性文档。

## 编写约定

- 文档内容应优先保持简洁、准确、结构清晰。
- 新增文档或修改文档时，应与当前仓库中的既有约定保持一致。

## CH32V208 存储映射约束

- 本项目默认面向 `CH32V208`。
- `CH32V208` 的实际可写 Flash 总量按 `448KB` 理解。
- 其中 `128KB` 为快速 Flash 区。
- 另有 `32KB` 可配置区域，可配置为 Flash 或 RAM。
- 另有 `32KB` 固定 RAM 区。
- 涉及链接脚本、烧录布局、内存分配、文档说明时，均应以上述映射为准。
- 不要仅依据 `OpenOCD` 日志中的 `flash size`、`ROM`、`RAM` 输出，直接推翻本项目约定的 `CH32V208` 存储映射；如工具输出与本项目约定不一致，应先视为工具识别或配置问题，再结合芯片手册、项目既有链接脚本和用户说明进行核对。

## 烧录约定

- 本仓库默认使用 `make flash` 调用项目内 `Scripts/wch_flash.py`，通过仓库内 `Scripts/WCH/CommunicationLib/libmcuupdate.so` 烧录 `Out/*.hex`。
- 默认烧录地址使用 `0x08000000` 的 Flash alias 地址。
- 如需启用 `SDI print`，可使用 `make flash-sdi`，或显式覆盖 `WCH_FLASH_SDI_PRINT=1`。
- `make flash-openocd` 使用仓库内 `Scripts/openocd/ch32v208-wch-riscv.cfg` 和重定位到 `0x08000000` 的 `Out/*.openocd.hex`。
- 当前实测中，OpenOCD 即使使用上述本地配置，仍会把目标错误识别为 `flash size = 160kbytes`、`ROM 128 kbytes RAM 64 kbytes`，并将 bank 上限卡在 `0x08028000`，导致访问 `0x08028000` 以上镜像地址时报 `no flash bank found`。遇到该现象时，应优先判断为工具识别或配置问题，而不是本项目链接脚本或 `CH32V208` 实际可写容量错误。
- 仓库内 `make flash-openocd` 仅保留为诊断入口；若镜像触及 `0x08028000` 及以上地址，应直接视为当前 OpenOCD 驱动能力不足，而不是继续尝试通过 Makefile、HEX 重定位或 cfg 中的 bank 参数修复。
- 如需把仓库内 `CommunicationLib` 副本做成无符号链接的单层目录，可执行 `make flatten-wch-lib`；当前目标目录为 `Scripts/WCH/CommunicationLib`。
