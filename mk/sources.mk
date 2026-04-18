APP_SRCS := \
	App/usb_mux_dev/main.c \
	App/usb_mux_dev/app_init.c

CORE_SRCS := \
	Core/core_riscv.c \
	Debug/debug.c \
	User/system_ch32v20x.c \
	User/ch32v20x_it.c

PERIPH_SRCS := \
	Peripheral/src/ch32v20x_gpio.c \
	Peripheral/src/ch32v20x_rcc.c \
	Peripheral/src/ch32v20x_usart.c \
	Peripheral/src/ch32v20x_misc.c

STARTUP_SRCS := \
	Startup/startup_ch32v20x_D8W.S

BLE_SRCS := \
	BLE/ble_task_scheduler.S
