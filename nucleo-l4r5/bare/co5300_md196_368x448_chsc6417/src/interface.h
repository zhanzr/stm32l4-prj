/*
  interface.h - LCD bus primitives for the co5300_md196_368x448_chsc6417
  project (nucleo-l4r5, CO5300 1.96" 368x448 module).

  PLAIN single-lane SPI on three wires - the panel is wired for one data
  line (QSPI_CS/QSPI_CLK/QSPI_IO0). The panel is driven the way the
  vendor SPI examples do it: every transaction is ONE chip-select frame
  carrying an 8-bit wrapped command:

      CS low
      byte 0x02            (transfer opcode: 1 data line)
      byte 0x00
      byte <cmd>           (the register/RAM command)
      byte 0x00
      ... parameter / pixel bytes ...
      CS high

  The 0x02 prefix IS the protocol header (the vendor's WriteComm sends
  exactly `02 00 <cmd> 00`; the vendor QSPI example's pixel burst uses
  the 4-line opcode 32h - with one data line wired, 02h is the
  single-line form of the same transfer). Keeping the pixel bytes in the
  same CS frame as their command is what lets a row be streamed without
  re-sending the header.

  TWO transports, selected at runtime (QSPI_SetMode) so the panel can be
  brought up along a ladder:

    SOFT - bit-banged SPI on plain GPIOs (CS/SCK/MOSI). The vendor TK499
           sequence, bit for bit (see interface.c). The forgiving link.
    HW   - the SPI1 peripheral, 8-bit frames, mode 3, MSB first.

  Signals - the same three pins for both transports:
    CS   = PA4  (driven by software; SPI1_NSS is NOT used)
    SCK  = PA5  (GPIO on SOFT; SPI1_SCK,  AF5 on HW)
    MOSI = PA7  (GPIO on SOFT; SPI1_MOSI, AF5 on HW)
  MISO - NOT CONNECTED. The driver is write-only by design: the CO5300
  wrapped protocol needs no readback (no status polling, no RAM read).

  Touch: CHSC6417 on hardware I2C1, SCL=PB8 / SDA=PB9 (unchanged).
*/

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include <stdint.h>
#include "stm32l4xx_hal.h"

/* Bus modes for QSPI_SetMode(). */
#define LCD_BUS_SOFT   0U  /* bit-banged GPIO SPI                */
#define LCD_BUS_HW     1U  /* SPI1 peripheral                    */

/* SPI1 baud prescaler. SPI1's kernel clock is PCLK2 = 120 MHz (APB2 /1),
 * so the ladder is:
 *
 *   /2 = 60 MHz   /4  = 30 MHz   /8  = 15 MHz   /16 = 7.5 MHz
 *   /32 = 3.75 MHz   /64 = 1.875 MHz   /128 = 937 kHz   /256 = 468 kHz
 *
 * CURRENT DEFAULT: /4 = 30 MHz. The CO5300 family allows ~47 MHz writes
 * at 3.3 V and the U5 port ran this panel at 40 MHz; the L4 prescaler is
 * powers of two, so /4 is the highest rung at or below that limit (there
 * is no 40/45 MHz step). /2 = 60 MHz was tried and is too fast for this
 * module: it corrupts the checkerboard.
 *
 * The runtime speed sweep in main.c walks the ladder up to this rate on
 * the board; set -DLCD_SPEED_SWEEP=0 for a clean run at a single rate. */
#ifndef LCD_SPI_PRESC
#define LCD_SPI_PRESC SPI_BAUDRATEPRESCALER_4
#endif

/* HW-path CS timing, in busy-loop counts (~40 ns each at 120 MHz). The
 * bit-bang path naturally leaves microseconds between CS and the first
 * clock edge (the settle loop, then the first byte's delay); the SPI
 * peripheral starts clocking within one SCK period of the first DR
 * write, which at 15 MHz is ~66 ns. Give the panel the same margins it
 * gets on the soft path. */
#ifndef LCD_HW_CS_SETUP
#define LCD_HW_CS_SETUP 25U    /* CS low  -> first SCK edge (~1 us)    */
#endif
#ifndef LCD_HW_CS_HOLD
#define LCD_HW_CS_HOLD 25U     /* last SCK edge -> CS high (~1 us)     */
#endif
#ifndef LCD_HW_CS_IDLE
#define LCD_HW_CS_IDLE 25U     /* CS high between frames (~1 us)       */
#endif

/* The wrapped-command header byte. 0x02 is the vendor's 1-data-line
 * opcode; the single-lane vendor example for this module uses it. */
#ifndef LCD_SPI_OPCODE
#define LCD_SPI_OPCODE 0x02U
#endif

/* Soft-SPI half-bit delay, in loop iterations (vendor TK499 default 4).
 * Raise to slow the bit-bang down; the achieved SCK is measured and
 * printed at init. */
#ifndef LCD_SOFT_SPI_DIV
#define LCD_SOFT_SPI_DIV 4U
#endif

/* Soft-SPI CS-high settle, in loop iterations. The vendor waits ~5 us
 * between frames (CS high, then low) before the next header. */
#ifndef LCD_SOFT_CS_SETTLE
#define LCD_SOFT_CS_SETTLE 200U
#endif

void          QSPI_Init(void);
void          QSPI_SetMode(uint8_t mode);     /* LCD_BUS_SOFT / LCD_BUS_HW */
uint8_t       QSPI_GetMode(void);

/* Runtime SCK change for the HW path (no-op for SOFT). `presc` is a
 * SPI_BAUDRATEPRESCALER_* token; if it differs from the current value,
 * SPI1 is torn down and re-initialised so CR1/BR takes effect. Used by
 * the speed sweep in main.c to find the ceiling without reflashing. */
void          QSPI_SetPrescaler(uint32_t presc);
uint32_t      QSPI_GetPrescaler(void);
void          QSPI_Cmd(uint8_t cmd);          /* wrapped, no data        */
void          QSPI_Write(uint8_t cmd, const uint8_t *data, uint32_t len);
void          QSPI_WritePixel(uint8_t cmd, const uint8_t *data, uint32_t len);
unsigned long QSPI_KHz(void);                 /* active SCK in kHz       */

#endif /* __INTERFACE_H */
