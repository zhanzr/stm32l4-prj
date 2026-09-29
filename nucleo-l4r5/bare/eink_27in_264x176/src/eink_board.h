/**
  * @file    eink_board.h
  * @brief   Board glue for the eink_27in_264x176 project (nucleo-l4r5).
  *
  *   MX_GPIO_Init()  - panel control pins (CS/RST/BUSY/PWR_EN/IO_EN/BRD/DISCHARGE)
  *   MX_SPI1_Init()  - SPI1 mode 0, MSB first, 30 MHz (APB2 120 MHz / 4)
  *   MX_ADC1_Init()  - ADC1 temp/VREFINT/VBAT (+ DMA1 channel 1)
  *   MX_DMA_Init()   - DMA1 clock + the ADC1 channel interrupt
  *   Board_Eink_Init() - Board_Init() plus the three initialisers above
  */

#ifndef __EINK_BOARD_H
#define __EINK_BOARD_H

void MX_GPIO_Init(void);
void MX_SPI1_Init(void);
void MX_ADC1_Init(void);
void MX_DMA_Init(void);

/* Board_Init() (clocks/LEDs/button/LPUART1 console) followed by the panel
 * peripherals, in the order the original main() used. */
void Board_Eink_Init(void);

#endif /* __EINK_BOARD_H */
