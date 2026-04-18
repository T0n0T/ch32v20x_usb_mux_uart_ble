#ifndef APP_USB_MUX_DEV_UART_PORT_CH32V20X_H
#define APP_USB_MUX_DEV_UART_PORT_CH32V20X_H

#include <stdint.h>

#include "vendor_proto.h"

vp_status_t UARTPort_Open(uint8_t logic_port, uint32_t baudrate, uint8_t data_bits, uint8_t parity, uint8_t stop_bits);
void UARTPort_Close(uint8_t logic_port);
void UARTPort_TryKickTx(uint8_t logic_port);

#endif
