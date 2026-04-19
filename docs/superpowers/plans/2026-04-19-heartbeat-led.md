# Heartbeat LED Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为当前固件增加一个编译期可配置的运行灯，默认使用 `PA0` 低电平点亮，并以非阻塞方式闪烁。

**Architecture:** 新增独立 `heartbeat` 模块承接运行灯初始化与轮询翻转逻辑，配置集中放在 `config` 目录。主循环只调用 `Heartbeat_Process()`，计时基于现有 RTC 计数换算毫秒，不引入新的中断或定时器依赖。

**Tech Stack:** C、CH32V20x 标准外设库、现有 RTC/TMOS 初始化流程、Make 构建系统

---

### Task 1: 接入运行灯配置与模块

**Files:**
- Modify: `App/usb_mux_dev/include/config.h`
- Create: `App/usb_mux_dev/include/heartbeat.h`
- Create: `App/usb_mux_dev/heartbeat.c`
- Modify: `App/usb_mux_dev/app_init.c`
- Modify: `App/usb_mux_dev/main.c`
- Modify: `Scripts/sources.mk`

- [ ] 添加编译期配置宏，默认设为 `PA0`、低电平点亮、500ms 翻转一次
- [ ] 实现 `Heartbeat_Init()` 和 `Heartbeat_Process()`，采用 RTC 计数做非阻塞节拍
- [ ] 在 `APP_Init()` 中调用初始化，在主循环中调用轮询
- [ ] 把新源文件加入构建列表

### Task 2: 构建验证

**Files:**
- Verify: `Out/ch32v208_usb_mux_ble_host.elf`

- [ ] 运行 `make` 验证工程可编译
- [ ] 如有告警或错误，修正到本次改动范围内
