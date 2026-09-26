/**
  * @file    uart_printf.c
  * @brief   LPUART1 printf implementation for the STM32L4R5ZIT6 "nucleo-l4r5" board.
  *
  * LPUART1 is on APB1 (60 MHz with the 120 MHz clock tree) and is wired to the
  * ST-Link's virtual COM port via PG7 (TX) and PG8 (RX), AF8. PG7/PG8 are in
  * the VddIO2 supply domain, so VddIO2 must be enabled before the GPIO config.
  * Output is 115200 8-N-1, blocking (polled) so nothing is dropped.
  */

#include "uart_printf.h"
#include "board.h"
#include "stm32l4xx_hal.h"

#include <stdio.h>
#include <stdarg.h>

static UART_HandleTypeDef hlpuart1;

/* ------------------------------------------------------------------------ */
/* printf() replacement for armclang builds (see cmake/printf_rename.h).
 * armclang would otherwise turn printf into ARMCLIB's __2printf ABI, which
 * cannot be linked against newlib. vprintf() is not specialized by armclang,
 * so this thin wrapper keeps the standard printf() working over the UART. */
int bench_printf(const char *fmt, ...)
{
    va_list args;
    int r;

    va_start(args, fmt);
    r = vprintf(fmt, args);
    va_end(args);
    return r;
}

/* ------------------------------------------------------------------------ */
void UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /* PG7/PG8 belong to the VddIO2 domain. */
    HAL_PWREx_EnableVddIO2();

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_LPUART1_CLK_ENABLE();

    /* LPUART1 kernel clock = PCLK1 (60 MHz). */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
    PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }

    /* PG7 = LPUART1_TX, PG8 = LPUART1_RX (AF8). */
    GPIO_InitStruct.Pin       = GPIO_PIN_7 | GPIO_PIN_8;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_LPUART1;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    hlpuart1.Instance          = LPUART1;
    hlpuart1.Init.BaudRate     = 115200U;
    hlpuart1.Init.WordLength   = UART_WORDLENGTH_8B;
    hlpuart1.Init.StopBits     = UART_STOPBITS_1;
    hlpuart1.Init.Parity       = UART_PARITY_NONE;
    hlpuart1.Init.Mode         = UART_MODE_TX_RX;
    hlpuart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&hlpuart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
int UART_PutChar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&hlpuart1, &c, 1U, 1000U);
    return ch;
}
