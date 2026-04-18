#include "debug.h"

void GPIO_Toggle_INIT (void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd (RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init (GPIOA, &GPIO_InitStructure);
}

int main (void) {
    NVIC_PriorityGroupConfig (NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
#if (SDI_PRINT == SDI_PR_OPEN)
    SDI_Printf_Enable();
#else
    USART_Printf_Init (115200);
#endif
    printf ("SystemClk:%d\r\n", SystemCoreClock);
    printf ("ChipID:%08x\r\n", DBGMCU_GetCHIPID());
    printf ("This is printf example\r\n");

    GPIO_Toggle_INIT();
    while (1) {
        Delay_Ms (500);
        GPIO_SetBits (GPIOA, GPIO_Pin_0);
        Delay_Ms (500);
        GPIO_ResetBits (GPIOA, GPIO_Pin_0);
    }
}
