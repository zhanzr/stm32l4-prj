/*
  interface.c - LCD bus primitives for the co5300_md196_368x448_chsc6417
  project (nucleo-l4r5, CO5300 1.96" 368x448 module). Identical bus to
  the nv3030b_md183_240x284_cst816d project (same three wires, same
  wrapped-command framing).

  PLAIN single-lane SPI. Two transports behind one byte API, selectable at
  runtime (QSPI_SetMode) so the panel can be brought up along a ladder:

    SOFT - bit-banged SPI on plain GPIOs, vendor TK499 sequence bit for
           bit: SCK falls, data is set, SCK rises (the panel samples on
           the rising edge). SCK is left high (mode-3 idle), so the next
           byte's first falling edge is a real edge.
    HW   - SPI1, 8-bit frames, mode 3 (CPOL=1/CPHA=2EDGE), MSB first,
           LCD_SPI_PRESC. CS stays a software GPIO in both cases.

  Wrapped command framing (vendor-verbatim): every transaction is one CS
  frame - CS high settle, CS low, then the 4-byte header
  `02 00 <cmd> 00`, then the parameter/pixel bytes, then CS high. The
  pixel bytes ride in the SAME frame as their command.

  Write-only by design: the driver never reads the panel and MISO is not
  connected. The CO5300 wrapped protocol needs no readback.

  Signals: CS = PA4, SCK = PA5 (SPI1_SCK AF5 on HW), MOSI = PA7
  (SPI1_MOSI AF5 on HW).
*/

#include <stdio.h>
#include "interface.h"
#include "board.h"

/* Spin guard for the raw SPI1 register loops (hw_tx/hw_reset). A byte at
 * the slowest prescaler takes ~2.7 us; this is orders of magnitude more,
 * so it only ever trips on a genuinely wedged peripheral. */
#define SPI_TX_GUARD 1000000U

/* Short busy-wait (~40 ns per iteration at 120 MHz) for the CS setup/hold
 * margins the bit-bang path gives the panel for free. */
static void cs_delay(uint32_t n)
{
    while (n-- != 0U)
    {
        ;
    }
}

static SPI_HandleTypeDef s_hspi;
static uint8_t s_mode = LCD_BUS_SOFT;

static uint32_t s_soft_khz;      /* measured soft SCK, kHz          */
static uint8_t  s_soft_cal;      /* soft SCK measured once          */
static uint8_t  s_spi_ready;     /* SPI1 initialized                */

/* Active HW prescaler. This, not s_hspi.Init, is the authority: the
 * handle is rebuilt from scratch every time the bus is re-armed, so a
 * value written straight into s_hspi.Init would be lost at the next
 * hw_pins_init(). Defaults to the build-time rate, changed at runtime by
 * QSPI_SetPrescaler() (the main.c speed sweep). */
static uint32_t s_presc = LCD_SPI_PRESC;

static void hw_pins_init(void);
static void hw_reset(void);

/* ---- pin accessors. Single BSRR store: no read-modify-write. ---- */
#define LCD_CS_HI()    (GPIOA->BSRR = GPIO_PIN_4)
#define LCD_CS_LO()    (GPIOA->BSRR = (uint32_t)GPIO_PIN_4 << 16)
#define LCD_SCK_HI()   (GPIOA->BSRR = GPIO_PIN_5)
#define LCD_SCK_LO()   (GPIOA->BSRR = (uint32_t)GPIO_PIN_5 << 16)
#define LCD_MOSI_HI()  (GPIOA->BSRR = GPIO_PIN_7)
#define LCD_MOSI_LO()  (GPIOA->BSRR = (uint32_t)GPIO_PIN_7 << 16)

void QSPI_SetMode(uint8_t mode)
{
    if ((mode <= LCD_BUS_HW) && (mode != s_mode))
    {
        s_mode = mode;
        /* Re-arm the bus for the new transport now, not on the next
         * write: re-muxing the pins on every transaction put that GPIO
         * churn inside the wrapped frame's timing. */
        QSPI_Init();
    }
}

uint8_t QSPI_GetMode(void)
{
    return s_mode;
}

/* Runtime SCK change for the HW path. SPI_CR1/BR is not write-protected
 * while SPE is set, but changing the divider mid-frame is exactly the
 * kind of edge the panel latches, so the peripheral is torn down (CS
 * high, SPE cleared) and re-initialised at the new rate. The caller is
 * expected to re-init the panel afterwards.
 *
 * The rate is kept in s_presc, not s_hspi.Init: hw_pins_init() re-creates
 * the handle's Init block from s_presc on every re-arm, so writing only
 * the handle would silently revert at the next mode switch. */
void QSPI_SetPrescaler(uint32_t presc)
{
    if (s_presc == presc)
    {
        return;
    }

    s_presc = presc;

    if (s_spi_ready != 0U)
    {
        LCD_CS_HI();
        (void)HAL_SPI_DeInit(&s_hspi);
        s_spi_ready = 0U;
    }

    if (s_mode == LCD_BUS_HW)
    {
        hw_pins_init();       /* re-init SPI1 at the new divider */
        hw_reset();
    }
}

uint32_t QSPI_GetPrescaler(void)
{
    return s_presc;
}

/* ---------------- pin ownership: GPIO  <->  SPI1 --------------------- */

/* Reconfigure SCK/MOSI as plain GPIO outputs (soft path). CS is always a
 * GPIO output, so it is configured once for both paths. */
static void soft_pins_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin   = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7;   /* CS, SCK, MOSI */
    HAL_GPIO_Init(GPIOA, &g);

    /* Mode-3 idle: CS high (deselected), SCK high, data low. */
    LCD_CS_HI();
    LCD_SCK_HI();
    LCD_MOSI_LO();
}

/* Configure SPI1 (idempotent) and hand SCK/MOSI to its AF. CS stays a
 * software GPIO. */
static void hw_pins_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* CS stays a plain output even on the hardware path. */
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin   = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOA, &g);
    LCD_CS_HI();

    if (s_spi_ready == 0U)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();

        s_hspi.Instance               = SPI1;
        s_hspi.Init.Mode              = SPI_MODE_MASTER;
        /* 2-LINES, not 1LINE, even though MISO is not wired. The L4 HAL
         * opens every SPI_DIRECTION_1LINE transfer with
         * __HAL_SPI_DISABLE() + SPI_1LINE_TX() (to set BIDIOE) and
         * re-enables SPE afterwards. SPE falling releases SCK/MOSI from
         * the peripheral mid-frame, so with CS already low the panel
         * latches that edge as an extra bit and the wrapped stream
         * shifts - exactly the corruption the soft path does not have.
         * 2LINES leaves SPE set for the whole transfer and gave the
         * same result on the U5 port; MISO is untouched (GPIO). */
        s_hspi.Init.Direction         = SPI_DIRECTION_2LINES;  /* TX-only use */
        s_hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
        s_hspi.Init.CLKPolarity       = SPI_POLARITY_HIGH;     /* mode 3  */
        s_hspi.Init.CLKPhase          = SPI_PHASE_2EDGE;
        s_hspi.Init.NSS               = SPI_NSS_SOFT;
        s_hspi.Init.BaudRatePrescaler = s_presc;
        s_hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
        s_hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
        s_hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
        s_hspi.Init.CRCPolynomial     = 7U;
        if (HAL_SPI_Init(&s_hspi) != HAL_OK)
        {
            Error_Handler();
        }
        s_spi_ready = 1U;
    }

    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    /* HIGH slew is comfortably above the default 15 MHz SCK (and 30 MHz
     * with LCD_SPI_PRESC changed). VERY_HIGH edges ring on flying wires
     * (see the st7789s port notes). */
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF5_SPI1;                        /* PA5, PA7 */
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &g);
}

/* ---------------- soft (bit-banged) SPI ------------------------------ */

/* One half SCK period. */
static void soft_spi_delay(void)
{
    volatile uint32_t n = (uint32_t)LCD_SOFT_SPI_DIV;

    while (n-- != 0U)
    {
        ;
    }
}

/* Shift one byte out, MSB first, mode 3 (vendor-verbatim sequence):
 * SCK falls, data is set, SCK rises (the panel samples here). SCK is
 * left high, the mode-3 idle level, so the next byte's first falling
 * edge is a real edge. */
static void soft_spi_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0; i < 8U; i++)
    {
        LCD_SCK_LO();                       /* falling edge: panel shifts */
        if ((b & 0x80U) != 0U) { LCD_MOSI_HI(); } else { LCD_MOSI_LO(); }
        soft_spi_delay();
        LCD_SCK_HI();                       /* rising edge: panel samples */
        soft_spi_delay();
        b = (uint8_t)(b << 1);
    }
}

/* Measure the real SCK the bit-bang loop produces (whole bytes timed, so
 * the GPIO stores and loop overhead are counted, not just the delay).
 * SCK toggling with CS high is harmless, so this runs before any frame. */
static void soft_spi_calibrate(void)
{
    uint32_t t0, t1, ms;
    uint32_t n = 20000UL;
    uint32_t i;

    if (s_soft_cal != 0U)
    {
        return;
    }

    t0 = HAL_GetTick();
    for (i = 0; i < n; i++)
    {
        soft_spi_byte(0xAAU);
    }
    t1 = HAL_GetTick();

    ms = t1 - t0;
    if (ms == 0U)
    {
        ms = 1U;
    }

    /* bytes/s = n / (ms/1000); SCK = bytes/s * 8. */
    s_soft_khz = (uint32_t)((uint64_t)n * 8ULL * 1000ULL /
                            ((uint64_t)ms * 1000ULL));
    s_soft_cal = 1U;
}

/* One wrapped soft-SPI transaction: CS high settle, CS low, the 4-byte
 * header 02 00 <cmd> 00, then the data bytes, all inside the same CS
 * frame (vendor-verbatim). */
static void soft_xfer(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    LCD_CS_HI();
    for (i = 0; i < LCD_SOFT_CS_SETTLE; i++)
    {
        ;
    }
    LCD_CS_LO();

    soft_spi_byte(LCD_SPI_OPCODE);
    soft_spi_byte(0x00U);
    soft_spi_byte(cmd);
    soft_spi_byte(0x00U);

    for (i = 0; i < len; i++)
    {
        soft_spi_byte(data[i]);
    }

    LCD_CS_HI();
}

/* ---------------- hardware SPI1 -------------------------------------- */

/* Blocking transmit that leaves the peripheral ENABLED (SPE stays set)
 * and discards the bytes the 2-line path clocks in. On the L4 this is
 * both faster and safer than HAL_SPI_Transmit(): that function gates
 * every write on TXE and finishes with a TXE/BSY wait, and in 2-line
 * mode also drains the RX FIFO per call - all of which costs a flag
 * poll per byte and gives the panel extra SCK time inside the CS frame
 * it is not listening for.
 *
 * SPE is asserted once by hw_pins_init()/hw_reset() and never cleared
 * while a frame is open, so the panel only ever sees real wrapped
 * bytes. Returns 0 on success. */
static uint8_t hw_tx(const uint8_t *p, uint32_t len)
{
    uint32_t i;
    uint32_t guard;

    for (i = 0; i < len; i++)
    {
        guard = SPI_TX_GUARD;
        while (((SPI1->SR & SPI_SR_TXE) == 0U) && (guard-- != 0U))
        {
            ;
        }
        if (guard == 0U)
        {
            return 1U;
        }
        *(__IO uint8_t *)&SPI1->DR = p[i];
    }

    /* Wait for the last byte to leave the shift register before the
     * caller drops CS - BSY clears only with the transmitter fully
     * idle. */
    guard = SPI_TX_GUARD;
    while (((SPI1->SR & SPI_SR_BSY) != 0U) && (guard-- != 0U))
    {
        ;
    }
    /* Discard one received byte (harmless in 2-line TX, keeps the RX
     * path from accumulating) and clear any stale overrun. */
    (void)SPI1->DR;
    (void)SPI1->SR;

    return (guard == 0U) ? 1U : 0U;
}

/* Put SPI1 back to the state a wrapped frame needs to start in: enabled,
 * transmitter idle, RX side clean. Called whenever the pins are handed
 * back to the peripheral. */
static void hw_reset(void)
{
    uint32_t guard = SPI_TX_GUARD;

    __HAL_SPI_ENABLE(&s_hspi);                 /* SPE = 1, stays set */

    while (((SPI1->SR & SPI_SR_BSY) != 0U) && (guard-- != 0U))
    {
        ;
    }
    (void)SPI1->DR;
}

/* One wrapped hardware transaction: CS low, the 4-byte header
 * 02 00 <cmd> 00, then the data, all inside the same CS frame. SPE is
 * never toggled while CS is low. */
static void hw_xfer(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[4];

    hdr[0] = LCD_SPI_OPCODE;
    hdr[1] = 0x00U;
    hdr[2] = cmd;
    hdr[3] = 0x00U;

    LCD_CS_HI();
    cs_delay(LCD_HW_CS_IDLE);
    LCD_CS_LO();
    cs_delay(LCD_HW_CS_SETUP);

    if (hw_tx(hdr, 4U) != 0U)
    {
        printf("[SPI] TX FAIL hdr cmd=%02X\r\n", (unsigned)cmd);
        LCD_CS_HI();
        return;
    }

    /* Data in whole-row chunks: the raw DR loop scales to any length,
     * this only keeps the CS frame open across a multi-row burst. */
    while (len != 0U)
    {
        uint16_t n = (len > 0xFFFFU) ? 0xFFFFU : (uint16_t)len;

        if (hw_tx((const uint8_t *)data, n) != 0U)
        {
            printf("[SPI] TX FAIL data cmd=%02X len=%lu\r\n",
                   (unsigned)cmd, (unsigned long)len);
            break;
        }
        data += n;
        len  -= n;
    }

    cs_delay(LCD_HW_CS_HOLD);
    LCD_CS_HI();
}

/* ---------------- public API ----------------------------------------- */

void QSPI_Write(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    if (s_mode == LCD_BUS_SOFT)
    {
        soft_pins_init();
        soft_xfer(cmd, data, len);
    }
    else
    {
        /* hw_pins_init() is idempotent, but the GPIO/AF and SPE state
         * only ever need to be established once per mode switch
         * (QSPI_SetMode/QSPI_Init); re-running them per transaction
         * would put that churn inside the wrapped frame. */
        hw_reset();          /* SPE on, transmitter idle, RX clean */
        hw_xfer(cmd, data, len);
    }
}

/* Pixel burst. Identical to a parameter write here: the whole point of
 * the wrapped framing is that pixels ride in the same CS frame as their
 * command, on the same single data line. Kept as a separate entry point
 * so lcd.c stays explicit about which transfers are pixel data. */
void QSPI_WritePixel(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    QSPI_Write(cmd, data, len);
}

void QSPI_Cmd(uint8_t cmd)
{
    QSPI_Write(cmd, NULL, 0U);
}

void QSPI_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    if (s_mode == LCD_BUS_SOFT)
    {
        soft_pins_init();
        soft_spi_calibrate();
    }
    else
    {
        hw_pins_init();
        hw_reset();
    }
}

/* Active SCK in kHz, e.g. 40000 = 40 MHz (for console/info prints). */
unsigned long QSPI_KHz(void)
{
    if (s_mode == LCD_BUS_SOFT)
    {
        return (unsigned long)s_soft_khz;
    }

    /* SPI1 is on APB2 (= 120 MHz here); the prescaler field holds a
     * two-level token, so decode it. (The U5 IP puts the field in
     * SPI_CFG1's MBR bits; the L4 IP keeps it in CR1's BR bits.) */
    {
        uint32_t div = 2U << ((s_presc & SPI_CR1_BR) >> SPI_CR1_BR_Pos);
        return (unsigned long)(HAL_RCC_GetPCLK2Freq() / div / 1000U);
    }
}
