#ifndef APP_USB_MUX_DEV_UART_MANAGER_H
#define APP_USB_MUX_DEV_UART_MANAGER_H

#include <stdint.h>

#include "vendor_proto.h"

typedef enum {
    UART_DISABLED = 0,
    UART_CLOSED,
    UART_OPENING,
    UART_OPEN,
    UART_DRAINING,
    UART_ERROR,
} uart_port_state_t;

void UartMgr_Init(void);
void UartMgr_Process(void);
void UartMgr_HandleCtrl(const vp_hdr_t *hdr, const uint8_t *payload, uint16_t payload_len);
vp_status_t UartMgr_WriteFromHost(uint8_t logic_port, const uint8_t *payload, uint16_t payload_len);
void UartMgr_IrqRxByte(uint8_t logic_port, uint8_t data);
int UartMgr_IrqTxNextByte(uint8_t logic_port, uint8_t *data);

#endif
