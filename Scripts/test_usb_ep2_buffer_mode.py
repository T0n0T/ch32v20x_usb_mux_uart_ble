#!/usr/bin/env python3
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
USB_DEV_LL = REPO_ROOT / "App/usb_mux_dev/usb/usb_dev_ll.c"


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    text = USB_DEV_LL.read_text(encoding="utf-8")

    require(
        "#define USB_EP2_IN_OFFSET            USBDEV_EP2_SIZE" in text,
        "EP2 IN staging offset was not found",
    )
    require(
        "memcpy(&g_ep2_dma[USB_EP2_IN_OFFSET]" in text,
        "EP2 IN data is not staged at USB_EP2_IN_OFFSET",
    )
    require(
        "USBFSD->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP2_TX_EN | USBFS_UEP2_BUF_MOD;" in text,
        "EP2 RX/TX shares one DMA register; using USB_EP2_IN_OFFSET requires USBFS_UEP2_BUF_MOD",
    )


if __name__ == "__main__":
    main()
