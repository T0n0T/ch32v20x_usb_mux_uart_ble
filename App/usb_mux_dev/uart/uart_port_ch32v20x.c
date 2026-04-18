#include "uart_port_ch32v20x.h"

#include <stddef.h>

#include "ch32v20x_gpio.h"
#include "ch32v20x_misc.h"
#include "ch32v20x_rcc.h"
#include "ch32v20x_usart.h"
#include "uart_manager.h"
#include "uart_map_config.h"

#if defined(__GNUC__) && defined(__riscv)
#define UART_IRQ_ATTR __attribute__((interrupt("WCH-Interrupt-fast")))
#else
#define UART_IRQ_ATTR
#endif

typedef struct {
    USART_TypeDef *instance;
    IRQn_Type irq;
    uint32_t periph_clock;
    uint8_t  use_apb2;
} uart_hw_desc_t;

static const uart_hw_desc_t g_uart_hw_desc[] = {
    {USART1, USART1_IRQn, RCC_APB2Periph_USART1, 1U},
    {USART2, USART2_IRQn, RCC_APB1Periph_USART2, 0U},
    {USART3, USART3_IRQn, RCC_APB1Periph_USART3, 0U},
    {UART4,  UART4_IRQn,  RCC_APB1Periph_UART4,  0U},
};

static int UARTPort_FindLogicPortByPhy(uint8_t phy_uart_id)
{
    uint8_t logic_port;

    for(logic_port = 0U; logic_port < UART_LOGIC_PORT_COUNT; ++logic_port)
    {
        if((g_uart_map_cfg[logic_port].enable != 0U) && (g_uart_map_cfg[logic_port].phy_uart_id == phy_uart_id))
        {
            return (int)logic_port;
        }
    }

    return -1;
}

static const uart_hw_desc_t *UARTPort_GetHwDesc(uint8_t logic_port)
{
    uint8_t phy_uart_id;

    if(logic_port >= UART_LOGIC_PORT_COUNT)
    {
        return 0;
    }

    phy_uart_id = g_uart_map_cfg[logic_port].phy_uart_id;
    if((phy_uart_id == 0U) || (phy_uart_id > (sizeof(g_uart_hw_desc) / sizeof(g_uart_hw_desc[0]))))
    {
        return 0;
    }

    return &g_uart_hw_desc[phy_uart_id - 1U];
}

static GPIO_TypeDef *UARTPort_GetGpio(uint32_t gpio_addr)
{
    return (GPIO_TypeDef *)(uintptr_t)gpio_addr;
}

static void UARTPort_EnableGpioClock(GPIO_TypeDef *gpio)
{
    if(gpio == GPIOA)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    }
    else if(gpio == GPIOB)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    }
    else if(gpio == GPIOC)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    }
    else if(gpio == GPIOD)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    }
}

static vp_status_t UARTPort_ApplyConfig(const uart_hw_desc_t *hw,
                                        const uart_map_cfg_t *cfg,
                                        uint32_t baudrate,
                                        uint8_t data_bits,
                                        uint8_t parity,
                                        uint8_t stop_bits)
{
    USART_InitTypeDef usart_cfg;
    GPIO_InitTypeDef gpio_cfg;
    GPIO_TypeDef *tx_gpio = UARTPort_GetGpio(cfg->tx_port);
    GPIO_TypeDef *rx_gpio = UARTPort_GetGpio(cfg->rx_port);
    NVIC_InitTypeDef nvic_cfg;

    if((hw == 0) || (cfg == 0) || (cfg->enable == 0U) || (tx_gpio == 0) || (rx_gpio == 0))
    {
        return VP_STATUS_ERR_UART_MAP_INVALID;
    }

    if(hw->use_apb2 != 0U)
    {
        RCC_APB2PeriphClockCmd(hw->periph_clock, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(hw->periph_clock, ENABLE);
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    UARTPort_EnableGpioClock(tx_gpio);
    UARTPort_EnableGpioClock(rx_gpio);

    gpio_cfg.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_cfg.GPIO_Pin = cfg->tx_pin;
    gpio_cfg.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(tx_gpio, &gpio_cfg);

    gpio_cfg.GPIO_Pin = cfg->rx_pin;
    gpio_cfg.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(rx_gpio, &gpio_cfg);

    USART_StructInit(&usart_cfg);
    usart_cfg.USART_BaudRate = baudrate;
    usart_cfg.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    usart_cfg.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_cfg.USART_WordLength = (data_bits == 9U) ? USART_WordLength_9b : USART_WordLength_8b;
    usart_cfg.USART_StopBits = (stop_bits == 2U) ? USART_StopBits_2 : USART_StopBits_1;

    if(parity == 1U)
    {
        usart_cfg.USART_Parity = USART_Parity_Odd;
    }
    else if(parity == 2U)
    {
        usart_cfg.USART_Parity = USART_Parity_Even;
    }
    else
    {
        usart_cfg.USART_Parity = USART_Parity_No;
    }

    USART_Cmd(hw->instance, DISABLE);
    USART_Init(hw->instance, &usart_cfg);
    USART_ITConfig(hw->instance, USART_IT_RXNE, ENABLE);
    USART_ITConfig(hw->instance, USART_IT_TXE, DISABLE);
    USART_Cmd(hw->instance, ENABLE);

    nvic_cfg.NVIC_IRQChannel = hw->irq;
    nvic_cfg.NVIC_IRQChannelCmd = ENABLE;
    nvic_cfg.NVIC_IRQChannelPreemptionPriority = 0U;
    nvic_cfg.NVIC_IRQChannelSubPriority = 0U;
    NVIC_Init(&nvic_cfg);

    return VP_STATUS_OK;
}

static void UARTPort_CommonIrq(uint8_t phy_uart_id)
{
    int logic_port = UARTPort_FindLogicPortByPhy(phy_uart_id);
    const uart_hw_desc_t *hw;

    if(logic_port < 0)
    {
        return;
    }

    hw = UARTPort_GetHwDesc((uint8_t)logic_port);
    if(hw == 0)
    {
        return;
    }

    if(USART_GetITStatus(hw->instance, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t)USART_ReceiveData(hw->instance);

        UartMgr_IrqRxByte((uint8_t)logic_port, data);
    }

    if(USART_GetITStatus(hw->instance, USART_IT_TXE) != RESET)
    {
        uint8_t data;

        if(UartMgr_IrqTxNextByte((uint8_t)logic_port, &data) == 0)
        {
            USART_SendData(hw->instance, data);
        }
        else
        {
            USART_ITConfig(hw->instance, USART_IT_TXE, DISABLE);
        }
    }
}

vp_status_t UARTPort_Open(uint8_t logic_port, uint32_t baudrate, uint8_t data_bits, uint8_t parity, uint8_t stop_bits)
{
    const uart_hw_desc_t *hw = UARTPort_GetHwDesc(logic_port);

    if(logic_port >= UART_LOGIC_PORT_COUNT)
    {
        return VP_STATUS_ERR_INVALID_PARAM;
    }

    return UARTPort_ApplyConfig(hw, &g_uart_map_cfg[logic_port], baudrate, data_bits, parity, stop_bits);
}

void UARTPort_Close(uint8_t logic_port)
{
    const uart_hw_desc_t *hw = UARTPort_GetHwDesc(logic_port);

    if(hw == 0)
    {
        return;
    }

    USART_ITConfig(hw->instance, USART_IT_RXNE, DISABLE);
    USART_ITConfig(hw->instance, USART_IT_TXE, DISABLE);
    USART_Cmd(hw->instance, DISABLE);
}

void UARTPort_TryKickTx(uint8_t logic_port)
{
    const uart_hw_desc_t *hw = UARTPort_GetHwDesc(logic_port);

    if(hw == 0)
    {
        return;
    }

    USART_ITConfig(hw->instance, USART_IT_TXE, ENABLE);
}

void USART1_IRQHandler(void) UART_IRQ_ATTR;
void USART2_IRQHandler(void) UART_IRQ_ATTR;
void USART3_IRQHandler(void) UART_IRQ_ATTR;
void UART4_IRQHandler(void) UART_IRQ_ATTR;

void USART1_IRQHandler(void)
{
    UARTPort_CommonIrq(1U);
}

void USART2_IRQHandler(void)
{
    UARTPort_CommonIrq(2U);
}

void USART3_IRQHandler(void)
{
    UARTPort_CommonIrq(3U);
}

void UART4_IRQHandler(void)
{
    UARTPort_CommonIrq(4U);
}
