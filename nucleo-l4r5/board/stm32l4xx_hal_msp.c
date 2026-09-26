/**
  * @file    stm32l4xx_hal_msp.c
  * @brief   HAL MSP (MCU Support Package) callbacks.
  *
  * The board layer configures each peripheral's GPIO/clock itself (see
  * board.c and uart_printf.c), so only the global MSP is needed here.
  */

#include "stm32l4xx_hal.h"

/**
  * @brief  Initializes the global MSP.
  */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}
