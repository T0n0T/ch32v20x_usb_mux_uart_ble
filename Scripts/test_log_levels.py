#!/usr/bin/env python3

from pathlib import Path
import re
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent
CONFIG_H = REPO_ROOT / "App/usb_mux_dev/include/config.h"
APP_LOG_H = REPO_ROOT / "App/usb_mux_dev/include/app_log.h"
USB_DEV_LL_C = REPO_ROOT / "App/usb_mux_dev/usb/usb_dev_ll.c"


def require(pattern: str, text: str, desc: str) -> None:
    if re.search(pattern, text, re.MULTILINE) is None:
        raise AssertionError(desc)


def main() -> int:
    config_text = CONFIG_H.read_text(encoding="utf-8")
    app_log_text = APP_LOG_H.read_text(encoding="utf-8")
    usb_text = USB_DEV_LL_C.read_text(encoding="utf-8")

    require(r"^#define\s+APP_LOG_MIN_LEVEL\s+APP_LOG_LEVEL_INFO\b",
            config_text,
            "default log level is not INFO")

    for level_name in ("DEBUG", "INFO", "WARNING", "ERROR"):
        require(rf"\bAPP_LOG_LEVEL_{level_name}\b",
                app_log_text + config_text,
                f"missing log level definition for {level_name}")

    for macro_name in (
        "APP_LOG_DEBUG",
        "APP_LOG_INFO",
        "APP_LOG_WARNING",
        "APP_LOG_ERROR",
        "APP_LOG_USB_DEBUG",
        "APP_LOG_USB_INFO",
        "APP_LOG_USB_WARNING",
        "APP_LOG_USB_ERROR",
    ):
        require(rf"\b{macro_name}\b", app_log_text, f"missing macro {macro_name}")

    expected_debug_messages = (
        "std req type=",
        "ep2 out len=",
        "ep2 in done len=",
        "hint send pending=",
        "frame send start len=",
        "irq ifg=",
    )
    for message in expected_debug_messages:
        require(rf'APP_LOG_USB_DEBUG\(".*{re.escape(message)}',
                usb_text,
                f"USB low-level message should be DEBUG: {message}")

    require(r"static void USBDEV_LogLineState\(const char \*tag\)",
            usb_text,
            "missing USBDEV_LogLineState helper")
    require(r'APP_LOG_USB_DEBUG\("%s ext=0x%08lX',
            usb_text,
            "USBDEV_LogLineState should log at DEBUG")

    expected_warning_messages = (
        "ep0 stall",
        "hint send reject",
        "frame send reject len=",
        "fifo overflow",
    )
    for message in expected_warning_messages:
        require(rf'APP_LOG_USB_WARNING\(".*{re.escape(message)}',
                usb_text,
                f"USB warning message should be WARNING: {message}")

    print("log level config ok")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
