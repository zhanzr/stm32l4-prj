/**
  * @file    main.h
  * @brief   Panel pin map for the eink_27in_264x176 project (nucleo-l4r5).
  *
  * Signal names and macros are kept identical to the original Keil project
  * (D:\stm32l4r5-demo, Inc/main.h) so that
  * src/pervasive_eink_hardware_driver.c can stay vendor-verbatim. The pins
  * themselves are unchanged - this port does not rewire the panel.
  *
  * Only the e-ink signals are defined here. The original CubeMX main.h also
  * carried USB-OTG, USART3 and LD/B1 defines generated for the whole
  * NUCLEO-L4R5ZI board; the parts still needed (LEDs, button, console) come
  * from the shared board layer (`board/board.h`), so redefining them here
  * would only invite drift.
  *
  *   CS  = PD14   RST = PF15   BUSY = PE13   PWR_EN = PF14
  *   IO_EN = PF13 BRD = PE9    DISCHARGE = PE11
  *   SPI1: SCK = PA5, MISO = PA6, MOSI = PA7 (AF5, mode 0, MSB first)
  */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32l4xx_hal.h"
#include "board.h"

/* --- E-ink panel control pins ------------------------------------------- */

/* Display I/O level-shifter enable (LOW = enabled) */
#define DispIoEn_Pin            GPIO_PIN_13
#define DispIoEn_GPIO_Port      GPIOF

/* Panel VCC load switch (HIGH = on) */
#define PWR_EN_Pin              GPIO_PIN_14
#define PWR_EN_GPIO_Port        GPIOF

/* Panel reset, active LOW (double pulse on power-on) */
#define DISP_RST_L_Pin          GPIO_PIN_15
#define DISP_RST_L_GPIO_Port    GPIOF

/* Border control (toggled to keep the border white) */
#define BRD_CTRL_Pin            GPIO_PIN_9
#define BRD_CTRL_GPIO_Port      GPIOE

/* Discharge control (pulsed HIGH during power-off) */
#define DISCHARGE_Pin           GPIO_PIN_11
#define DISCHARGE_GPIO_Port     GPIOE

/* Panel busy status input */
#define DISP_BUSY_Pin           GPIO_PIN_13
#define DISP_BUSY_GPIO_Port     GPIOE

/* SPI chip select, software-controlled (active LOW) */
#define SPI_CS_Pin              GPIO_PIN_14
#define SPI_CS_GPIO_Port        GPIOD

/* --- On-board LEDs / button (board layer names, kept for the demo loop) --- */
#define LD1_Pin                 LED1_Pin
#define LD1_GPIO_Port           LED1_GPIO_Port
#define LD2_Pin                 LED2_Pin
#define LD2_GPIO_Port           LED2_GPIO_Port
#define LD3_Pin                 LED3_Pin
#define LD3_GPIO_Port           LED3_GPIO_Port

#define LD1_ON()                LED1_ON()
#define LD1_OFF()               LED1_OFF()
#define LD1_TOGGLE()            LED1_TOGGLE()
#define LD2_ON()                LED2_ON()
#define LD2_OFF()               LED2_OFF()
#define LD2_TOGGLE()            LED2_TOGGLE()
#define LD3_ON()                LED3_ON()
#define LD3_OFF()               LED3_OFF()
#define LD3_TOGGLE()            LED3_TOGGLE()

#define B1_Pin                  BTN_Pin
#define B1_GPIO_Port            BTN_GPIO_Port

#endif /* __MAIN_H */
