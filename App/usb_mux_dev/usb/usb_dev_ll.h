#ifndef APP_USB_MUX_DEV_USB_DEV_LL_H
#define APP_USB_MUX_DEV_USB_DEV_LL_H

#include <stdint.h>

#include "../proto/vendor_proto.h"

#define USBDEV_EP0_SIZE 64U
#define USBDEV_EP1_SIZE 8U
#define USBDEV_EP2_SIZE 64U
#define USBDEV_EP3_SIZE 64U

void USBMUX_DeviceInit(void);

int USBDEV_IsConfigured(void);
uint8_t USBDEV_GetConfiguration(void);

int USBDEV_CanSendHint(void);
int USBDEV_CanSendFrame(void);
int USBDEV_SendHint(const vp_irq_hint_t *hint);
int USBDEV_SendFrame(const uint8_t *data, uint16_t len);

#endif
