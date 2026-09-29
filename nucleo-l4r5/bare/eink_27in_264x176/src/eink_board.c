/**
  * @file    eink_board.c
  * @brief   Panel pin map, SPI1 and ADC1/DMA setup for the
  *          eink_27in_264x176 project (nucleo-l4r5).
  *
  * Replaces the original Keil project's gpio.c / spi.c / adc.c / dma.c. The
  * electrical setup is unchanged (same pins, same SPI mode/speed); what was
  * dropped is the CubeMX scaffolding this demo never used - USB-OTG pins,
  * USART3, and the SPI1 TX/RX DMA streams (the panel bus is blocking SPI, so
  * those streams and their DMA1_Channel2/3 interrupts were dead code).
  *
  * The clock tree is NOT set up here: board/board.c already brings the part
  * up at 120 MHz with APB2 = 120 MHz, which is what the original
  * SystemClock_Config_120() produced, so SPI1 lands on the same 30 MHz SCK.
  */

#include "main.h"
#include "stm32l4xx_hal.h"
#include "gpio.h"
#include "spi.h"
#include "dma.h"

/* ------------------------------------------------------------------------ */
/* Panel control GPIO                                                        */
/* ------------------------------------------------------------------------ */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PF (DispIoEn/PWR_EN/DISP_RST_L), PE (BRD_CTRL/DISCHARGE/DISP_BUSY) and
     * PD (SPI_CS). Ports G/B/C and VddIO2 are already handled by Board_Init()
     * (console + LEDs + button). */
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* Driven-inactive levels before the pins become outputs, so a glitch
     * cannot switch panel power on or assert reset. */
    HAL_GPIO_WritePin(GPIOF, DispIoEn_Pin | PWR_EN_Pin | DISP_RST_L_Pin,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, BRD_CTRL_Pin | DISCHARGE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);

    /* Push-pull outputs with the output speed the original used (LOW): the
     * panel control lines are slow, only SCK/MOSI need a fast edge. */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = DispIoEn_Pin | PWR_EN_Pin | DISP_RST_L_Pin;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BRD_CTRL_Pin | DISCHARGE_Pin;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SPI_CS_Pin;
    HAL_GPIO_Init(SPI_CS_GPIO_Port, &GPIO_InitStruct);

    /* DISP_BUSY: plain input, no pull (the panel drives it). */
    GPIO_InitStruct.Pin  = DISP_BUSY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DISP_BUSY_GPIO_Port, &GPIO_InitStruct);

    /* The original also configured B1 as an EXTI-rising input. Simple demos
     * here poll the button through BTN_PRESSED(), which needs just an input,
     * and board.c has already set PC13 up - so nothing to do. */
}

/* ------------------------------------------------------------------------ */
/* SPI1 - the panel bus                                                      */
/* ------------------------------------------------------------------------ */
SPI_HandleTypeDef hspi1;

void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;    /* mode 0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    /* APB2 = 120 MHz here, /4 = 30 MHz - identical to the original project
     * (its SystemClock_Config_120 also produced PCLK2 = 120 MHz, and it used
     * the same /4). The vendor driver clocks one byte per HAL call, so this
     * is far below anything the panel or the HAL cares about. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7U;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *spiHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (spiHandle->Instance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();
        /* GPIOA must be clocked before its registers can be written. The
         * original project got this from its CubeMX MX_GPIO_Init(), which
         * enabled every port on the board; this port's MX_GPIO_Init only
         * touches the panel control pins (PORTF/E/D), and nothing else in
         * the board layer enables GPIOA - so without this line
         * HAL_GPIO_Init() below is a silent no-op and PA5/PA6/PA7 stay in
         * their reset state (no clock, no data to the panel, nothing drawn). */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA5 = SCK, PA6 = MISO, PA7 = MOSI (AF5). The original used
         * VERY_HIGH slew; the bus runs at 30 MHz into the panel's ribbon, so
         * HIGH keeps the edges clean without the ringing VERY_HIGH can add on
         * jumper wiring. */
        GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/* ------------------------------------------------------------------------ */
/* ADC1 + DMA (internal channels only - no external analog wiring)           */
/* ------------------------------------------------------------------------ */
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

/* Stays in the project because the original demo prints this buffer every
 * loop iteration even though it leaves HAL_ADC_Start_DMA() commented out (so
 * the values read as 0 0 0). The init is ported so that uncommenting that one
 * call in main.c yields real numbers - which needs two fixes the original
 * Keil build got away with only because the conversion never ran:
 *
 *   - the ADC kernel clock is configured here (SYSCLK, sync mode). The
 *     original relied on SystemClock_Config()'s PLLSAI1 setup, which is
 *     bypassed in favour of the board's clock tree.
 *   - HAL_ADCEx_Calibration_Start() is called. The L4 ADC needs a
 *     calibration before the first conversion; without it the first values
 *     are meaningless.
 */
void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    __HAL_RCC_ADC_CLK_ENABLE();

    /* ADC kernel clock = SYSCLK (120 MHz); the prescaler below divides by 4
     * (120/4 = 30 MHz, inside the 80 MHz ADC limit). */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection    = RCC_ADCCLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.NbrOfConversion       = 3;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfDiscConversion   = 1;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode      = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        Error_Handler();
    }

    /* Channel order matches the original: temp, then VREFINT, then VBAT -
     * i.e. g_adcBuf[0..2]. */
    sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;

    sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
    sConfig.Rank    = 1;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_VREFINT;
    sConfig.Rank    = 2;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_VBAT;
    sConfig.Rank    = 3;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adcHandle)
{
    if (adcHandle->Instance == ADC1)
    {
        __HAL_RCC_ADC_CLK_ENABLE();

        hdma_adc1.Instance                 = DMA1_Channel1;
        hdma_adc1.Init.Request             = DMA_REQUEST_ADC1;
        hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        hdma_adc1.Init.Mode                = DMA_CIRCULAR;
        hdma_adc1.Init.Priority            = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(adcHandle, DMA_Handle, hdma_adc1);
    }
}

void MX_DMA_Init(void)
{
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* Only the ADC channel: the panel bus is blocking SPI, so the original's
     * SPI1 TX/RX streams (DMA1_Channel2/3) are not ported. */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/* DMA1 channel 1 - ADC1. Same handler the original project had; needed as
 * soon as HAL_ADC_Start_DMA() is enabled in main.c. */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

/* ------------------------------------------------------------------------ */
/* One-call bring-up, in the original main()'s order                         */
/* ------------------------------------------------------------------------ */
void Board_Eink_Init(void)
{
    Board_Init();       /* clocks (120 MHz), LEDs, button, LPUART1 console */

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_SPI1_Init();
}
