/**
  * @file    board.c
  * @brief   Board init for the STM32L4R5ZIT6 "nucleo-l4r5" board (NUCLEO-L4R5ZI).
  *
  * Clock tree (HSI = 16 MHz, no external crystal needed):
  *   PLLM=2   -> PLL input    8 MHz
  *   PLLN=30  -> VCO        240 MHz
  *   PLLR=2   -> SYSCLK     120 MHz
  *   PLLP=2   -> 120 MHz, PLLQ=2 -> 120 MHz (not used)
  *   HSI48 is left off; the USB FS clock is not needed by these projects.
  *   AHB=120 MHz, APB1=60 MHz (/2), APB2=120 MHz (/1)
  *   Flash latency 5 wait states, regulator voltage scale 1 + boost.
  *
  * SystemClock_Config() is declared weak so an individual project can bring
  * in its own clock setup without affecting the default 120 MHz used by
  * every other project.
  */

#include "board.h"
#include "uart_printf.h"
#include "swv_printf.h"

/* ------------------------------------------------------------------------ */
__attribute__((weak)) void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Regulator voltage scale 1 + boost is required above 80 MHz. */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
    {
        Error_Handler();
    }

    /* HSI 16 MHz -> PLL -> 120 MHz. */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 2U;
    RCC_OscInitStruct.PLL.PLLN            = 30U;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
static void GPIO_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* LD1 PC7, LD2 PB7, LD3 PB14: push-pull output, HIGH = ON. Start OFF. */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = LED1_Pin;
    HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = LED2_Pin | LED3_Pin;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    LED1_OFF();
    LED2_OFF();
    LED3_OFF();
}

/* ------------------------------------------------------------------------ */
static void GPIO_Button_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* B1 PC13: pressed shorts the pin to GND (external pull-up on the board). */
    GPIO_InitStruct.Pin  = BTN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BTN_GPIO_Port, &GPIO_InitStruct);
}

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
    SystemClock_Config();
    GPIO_LED_Init();
    GPIO_Button_Init();
    UART_Init();
    SWV_Init();
}

/* ------------------------------------------------------------------------ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        /* Blink LD3 red as a fatal-error indicator. */
        LED3_TOGGLE();
        for (volatile uint32_t i = 0; i < 1000000UL; i++) { }
    }
}
