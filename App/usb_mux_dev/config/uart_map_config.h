#ifndef APP_USB_MUX_DEV_UART_MAP_CONFIG_H
#define APP_USB_MUX_DEV_UART_MAP_CONFIG_H

#include <stdint.h>

#include "board_caps.h"

#define UART_LOGIC_PORT_COUNT APP_UART_PORT_COUNT

typedef struct {
    uint8_t  enable;
    uint8_t  phy_uart_id;
    uint32_t tx_port;
    uint16_t tx_pin;
    uint32_t rx_port;
    uint16_t rx_pin;
} uart_map_cfg_t;

extern const uart_map_cfg_t g_uart_map_cfg[UART_LOGIC_PORT_COUNT];

#endif
