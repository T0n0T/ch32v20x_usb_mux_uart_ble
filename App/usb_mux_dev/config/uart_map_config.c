#include "uart_map_config.h"

#include "ch32v20x_gpio.h"

#define UART_PORT_ADDR(port) ((uint32_t)(uintptr_t)(port))

const uart_map_cfg_t g_uart_map_cfg[UART_LOGIC_PORT_COUNT] = {
    {1U, 1U, UART_PORT_ADDR(GPIOA), GPIO_Pin_9,  UART_PORT_ADDR(GPIOA), GPIO_Pin_10},
    {1U, 2U, UART_PORT_ADDR(GPIOA), GPIO_Pin_2,  UART_PORT_ADDR(GPIOA), GPIO_Pin_3},
    {1U, 3U, UART_PORT_ADDR(GPIOB), GPIO_Pin_10, UART_PORT_ADDR(GPIOB), GPIO_Pin_11},
    {1U, 4U, UART_PORT_ADDR(GPIOC), GPIO_Pin_10, UART_PORT_ADDR(GPIOC), GPIO_Pin_11},
};
