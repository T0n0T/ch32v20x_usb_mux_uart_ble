#!/usr/bin/env python3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
USB_DEV_LL = REPO_ROOT / "App/usb_mux_dev/usb/usb_dev_ll.c"


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    text = USB_DEV_LL.read_text(encoding="utf-8")

    for name in ("g_ep0_dma", "g_ep1_dma", "g_ep2_dma", "g_ep3_dma"):
        require(
            f"static __attribute__((aligned(64))) uint8_t {name}" in text,
            f"{name} must be 64-byte aligned for CH32 USBFS endpoint DMA",
        )

    require(
        "static __attribute__((aligned(64))) uint8_t g_ep2_dma[USBDEV_EP2_SIZE];" in text,
        "EP2 IN should use its own 64-byte DMA buffer",
    )
    require(
        "static __attribute__((aligned(64))) uint8_t g_ep3_dma[USBDEV_EP3_SIZE];" in text,
        "EP3 OUT should use its own 64-byte DMA buffer",
    )
    require(
        "0x07, 0x05, 0x03, 0x02, USBDEV_EP3_SIZE" in text,
        "configuration descriptor must advertise EP3 OUT for host-to-device frames",
    )
    require(
        "USBFSD->UEP2_3_MOD = USBFS_UEP2_TX_EN | USBFS_UEP3_RX_EN;" in text,
        "USBFS frame endpoints should follow the CH372Device-style EP2 IN + EP3 OUT split",
    )
    require(
        "USBFSD->UEP3_DMA = (uint32_t)(uintptr_t)g_ep3_dma;" in text,
        "EP3 OUT DMA is not configured",
    )
    require(
        "USBFSD->UEP2_TX_CTRL = (uint8_t)((USBFSD->UEP2_TX_CTRL & USBFS_UEP_T_TOG)" in text,
        "EP2 IN completion must preserve DATA toggle when returning to NAK",
    )
    require(
        "USBFSD->UEP3_RX_CTRL = (uint8_t)((USBFSD->UEP3_RX_CTRL & USBFS_UEP_R_TOG)" in text,
        "EP3 OUT re-arm must preserve DATA toggle when returning to ACK",
    )


if __name__ == "__main__":
    main()
