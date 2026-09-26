/**
  * @file    board.h
  * @brief   Board support for the STM32L4R5ZIT6 "nucleo-l4r5" board
  *          (NUCLEO-L4R5ZI, STM32L4R5ZIT6 in LQFP144).
  *
  * LEDs:    LD1 green  PC7,  LD2 blue PB7,  LD3 red PB14 - all HIGH-active
  *          (the NUCLEO drives them directly; HIGH = ON).
  * Button:  B1 user, PC13, active-low (pressed = GND).
  * Console: LPUART1 PG7 (TX) / PG8 (RX), AF8, 115200 8-N-1, wired to the
  *          ST-Link virtual COM port (VCP). PG7/PG8 live in the VddIO2 domain,
  *          so Board_Init() enables VddIO2 before configuring the pins.
  */

#ifndef __BOARD_H__
#define __BOARD_H__

#include "stm32l4xx_hal.h"

/* --- LEDs (high-active) ---------------------------------------------------- */
#define LED1_Pin        GPIO_PIN_7
#define LED1_GPIO_Port  GPIOC
#define LED2_Pin        GPIO_PIN_7
#define LED2_GPIO_Port  GPIOB
#define LED3_Pin        GPIO_PIN_14
#define LED3_GPIO_Port  GPIOB

#define LED1_ON()       HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET)
#define LED1_OFF()      HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET)
#define LED1_TOGGLE()   HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin)

#define LED2_ON()       HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET)
#define LED2_OFF()      HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET)
#define LED2_TOGGLE()   HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin)

#define LED3_ON()       HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET)
#define LED3_OFF()      HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET)
#define LED3_TOGGLE()   HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin)

/* --- User button (B1 = PC13, pressed -> GND) ------------------------------ */
#define BTN_Pin         GPIO_PIN_13
#define BTN_GPIO_Port   GPIOC

#define BTN_PRESSED()   (HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET)

/* --- Init ------------------------------------------------------------------ */
void Board_Init(void);          /* clocks (120 MHz), LEDs, button, console (LPUART1 PG7/PG8) */
void SystemClock_Config(void);  /* HSI16 -> PLL -> 120 MHz */
void Error_Handler(void);

#endif /* __BOARD_H__ */
