#ifndef APP_USB_MUX_DEV_USB_RX_FSM_H
#define APP_USB_MUX_DEV_USB_RX_FSM_H

#include <stdint.h>

typedef enum {
    USB_RX_IDLE = 0,
    USB_RX_HEADER,
    USB_RX_PAYLOAD,
    USB_RX_DISPATCH,
    USB_RX_DROP,
} usb_rx_state_t;

void USBRX_Init(void);
void USBRX_PushBytes(const uint8_t *data, uint16_t len);
void USBRX_Process(void);

#endif
