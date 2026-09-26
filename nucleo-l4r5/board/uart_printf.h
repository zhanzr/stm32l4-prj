/**
  * @file    uart_printf.h
  * @brief   UART printf backend for the STM32L4R5ZIT6 "nucleo-l4r5" board.
  *
  * printf() output is redirected to LPUART1 (PG7 = TX, PG8 = RX, AF8),
  * 115200 8-N-1, wired to the ST-Link's virtual COM port (VCP).
  */

#ifndef __UART_PRINTF_H__
#define __UART_PRINTF_H__

void UART_Init(void);
int  UART_PutChar(int ch);

/*
 * printf() replacement for armclang builds (see cmake/printf_rename.h).
 * Defined in uart_printf.c; forwards to newlib vprintf() so output reaches
 * the UART through _write() -> UART_PutChar().
 */
int bench_printf(const char *fmt, ...);

#endif /* __UART_PRINTF_H__ */
