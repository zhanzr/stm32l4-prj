Board (Nucleo-L4R5ZI) → E-ink module connections, per `Inc/main.h` pin defines and the driver macros in `Inc/pervasive_eink_hardware_driver.h`:

| Signal | MCU Pin | Direction | Function | Driver macro |
|---|---|---|---|---|
| SPI1_SCK | PA5 | MCU → EPD | SPI clock (mode 0, MSB first) | — |
| SPI1_MOSI | PA7 | MCU → EPD | SPI data to EPD | — |
| SPI1_MISO | PA6 | EPD → MCU | SPI data from EPD (register reads) | — |
| SPI_CS | PD14 | MCU → EPD | Chip select, software-controlled (active LOW) | `ST_EINK_CsLow/High` |
| DISP_RST_L | PF15 | MCU → EPD | Reset (active LOW, double pulse on power-on) | `ST_EINK_RstLow/High` |
| DISP_BUSY | PE13 | EPD → MCU | Busy status input | read in `Pv_EINK_InitDriver()` |
| PWR_EN | PF14 | MCU → EPD | Panel VCC load switch (HIGH = on) | `EINK_VccOn/Off` |
| DispIoEn | PF13 | MCU → EPD | Display I/O level-shifter enable (LOW = enabled) | `ST_EINK_EnableIO` |
| BRD_CTRL | PE9 | MCU → EPD | Border control (toggled to keep border white) | `ST_EINK_BorderLow/High` |
| DISCHARGE | PE11 | MCU → EPD | Discharge control (pulsed HIGH during power-off) | `ST_EINK_DischargeLow/High` |

Notes:
- CS, RST, border, and discharge are plain push-pull GPIO (`GPIO_SPEED_FREQ_LOW`); CS is manually framed around each register index/data transaction (`Pv_EINK_SendData`, Src/pervasive_eink_hardware_driver.c:117).
- SPI1 runs in full-duplex 2-lines mode with hardware NSS disabled — only PD14 GPIO chip-select matters.
- Power-off sequence: VCC off → border LOW → CS/RST LOW → discharge HIGH (30 ms) → discharge LOW (Src/pervasive_eink_hardware_driver.c:1026-1036).
- No UART/I2C connection to the panel; on-board LEDs LD1 (PC7) / LD2 (PB7) are only status indicators, not part of the e-ink interface.