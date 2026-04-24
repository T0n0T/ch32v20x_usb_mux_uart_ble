# Host libusb 用户态驱动实施计划

**目标：** 在仓库根目录新增 `Host` 子项目，并作为 git submodule 挂载；子项目实现面向当前 `CH32V208 USB Vendor UART + BLE Host` 固件的 libusb 用户态驱动基础。

**架构：** `Host` 是独立 C/CMake 工程。协议编解码、libusb 设备传输、SYS/UART 高层 API 分层实现，CLI 只调用公开 API。后续迁移内核态时，协议层和通道语义可复用，传输层替换为 URB、tty 与内核同步原语。

**技术栈：** C11、CMake、pkg-config、libusb-1.0、CTest。

## 文件结构

- `Host/include/ch32v208_mux/proto.h`：协议常量、帧头、状态码、编解码 API。
- `Host/include/ch32v208_mux/device.h`：libusb 设备打开、关闭、帧读写、hint 读取 API。
- `Host/include/ch32v208_mux/uart.h`：SYS/UART 便捷 API。
- `Host/src/proto.c`：小端序列化、CRC16、帧头校验、帧构造。
- `Host/src/device.c`：libusb 初始化、VID/PID 查找、接口 claim、bulk/interrupt 传输。
- `Host/src/uart.c`：SYS 查询、心跳、UART 打开/关闭、UART 数据收发。
- `Host/tools/ch32v208-mux-cli.c`：命令行自检工具。
- `Host/tests/test_proto.c`：无硬件协议单元测试。
- `Host/Docs/USERSPACE_DRIVER.md`：中文说明、迁移边界与使用方法。
- `.gitmodules`：父仓库 submodule 登记。

## 执行步骤

1. 创建 `Host` 独立 git 仓库和 CMake 工程骨架。
2. 先添加 `test_proto.c`，覆盖帧头编码、CRC 错误检测、payload 上限检测。
3. 实现 `proto.c` 到单元测试通过。
4. 添加 `device.c`，只承担 libusb 传输，不混入业务协议。
5. 添加 `uart.c`，封装当前固件已实现的 `SYS` 与 `UART` 命令。
6. 添加 CLI，自检默认执行 `dev-info`、`caps`、`heartbeat`，可选 UART 操作。
7. 补充 `Host/Docs/USERSPACE_DRIVER.md` 和 `Host/README.md`。
8. 在 `Host` 内提交初始版本。
9. 父仓库写入 `.gitmodules`，将 `Host` 挂为 submodule gitlink。
10. 运行 `cmake -S Host -B Host/build`、`cmake --build Host/build`、`ctest --test-dir Host/build --output-on-failure` 验证。
