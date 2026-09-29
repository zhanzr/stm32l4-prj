# eink_27in_264x176 - Pervasive Displays 2.7" E-INK demo (264x176)

Drives the **Pervasive Displays 2.7" E-INK** panel (264x176, 1 bpp, the
Cypress/Pervasive G2 driver protocol) on the **nucleo-l4r5** board
(STM32L4R5ZIT6 @ 120 MHz), alternating two full-screen images with a
2-stage update.

Ported from the Keil MDK project `D:\stm32l4r5-demo` ("Eink Driver Demo for
L4R5 Nucleo board", `l4r5_t1`). The panel, its wiring, the vendor driver and
the demo behaviour are unchanged - only the surrounding environment was
replaced (see the port notes below).

## Wiring (unchanged from the original)

| Signal | MCU pin | Direction | Function |
| ------ | ------- | --------- | -------- |
| SPI1_SCK   | **PA5** | MCU → EPD | SPI clock (mode 0, MSB first, 30 MHz) |
| SPI1_MOSI  | **PA7** | MCU → EPD | SPI data to EPD |
| SPI1_MISO  | **PA6** | EPD → MCU | SPI data from EPD (driver ID / breakage / DC reads) |
| SPI_CS     | **PD14** | MCU → EPD | chip select, software-controlled (active LOW) |
| DISP_RST_L | **PF15** | MCU → EPD | reset (active LOW, double pulse on power-on) |
| DISP_BUSY  | **PE13** | EPD → MCU | busy status input |
| PWR_EN     | **PF14** | MCU → EPD | panel VCC load switch (HIGH = on) |
| DispIoEn   | **PF13** | MCU → EPD | I/O level-shifter enable (LOW = enabled) |
| BRD_CTRL   | **PE9**  | MCU → EPD | border control (toggled to keep the border white) |
| DISCHARGE  | **PE11** | MCU → EPD | discharge control (pulsed HIGH during power-off) |

MISO matters here: unlike the LCD projects in this repo, the E-INK driver
reads the panel back (driver ID, breakage, DC level), so SPI1 runs
full-duplex 2-lines. `DISP_BUSY` and the read-back are what make a wiring
fault visible as `ERROR_ID` rather than a blank screen.

## Port notes (what changed vs. the Keil project)

1. **Toolchain / build.** The Keil MDK project (`MDK-ARM/l4r5_t1.uvprojx`,
   armclang + Armlink + CubeMX-generated peripheral files) is replaced by
   this repo's CMake/Ninja + GCC `arm-none-eabi` setup, and by the shared
   board layer in `../../board/`. `bash build.sh` then `ninja -C build flash`
   works exactly like the other projects.
2. **Debug output is the UART, not ITM.** The original retargeted `printf()`
   to ITM/SWO through the Keil CMSIS-Compiler *STDOUT: ITM* component. This
   port has **no ITM or SWO dependency at all**: `printf()` goes through the
   board's `_write()` (`board/syscalls.c`) to **LPUART1 PG7/PG8**, i.e. the
   ST-Link virtual COM port at 115200 8-N-1. Because the console is now a
   real port, the port also *prints more*: the panel init path reports the
   `pv_eink_status_t` code on failure (`ERROR_ID` = panel not answering on
   MISO, `ERROR_BUSY` / `ERROR_BREAKAGE` / `ERROR_CHARGEPUMP` = panel side),
   which the original collapsed into a single `__SEV()` halt.
3. **Images replaced.** The original `logo` / `image2` arrays in
   `Src/resource.c` are replaced by `board_0` / `board_1`, converted from
   `../eink_assets/board_0.bmp` and `board_1.bmp` by
   `tools/bmp_to_eink.py`. See `src/resource.c` (generated) and the
   "Images" section below.

Dropped along the way (all unused by the demo): the CubeMX USB-OTG, USART3
and SWO pin setup; the SPI1 TX/RX DMA streams (the panel bus is blocking SPI,
so `DMA1_Channel2/3` and their interrupts were dead); two RAM frame buffers
the original declared but never touched. The clock tree is the board's
(120 MHz from HSI, APB2 = 120 MHz), which produces the same SPI1 rate
(120/4 = **30 MHz**) the original's `SystemClock_Config_120()` did.

### GPIOA clock: the one thing MX_GPIO_Init must still do

The original's CubeMX `MX_GPIO_Init()` enabled **every** GPIO port on the
board, including **GPIOA** - and that was the *only* place the SPI1 pins'
port clock was enabled (its `HAL_SPI_MspInit` only enables SPI1 itself).

This port's `MX_GPIO_Init()` only sets up the panel control pins (PORTF/E/D),
so GPIOA would never be clocked. With the clock gated, `HAL_GPIO_Init(GPIOA,
...)` is a **silent no-op**: PA5/PA6/PA7 stay in their reset state, the panel
gets no clock and no data, and the demo draws nothing - while the SPI
peripheral reports no error at all. `HAL_SPI_MspInit()` in `src/eink_board.c`
now enables GPIOA next to the AF configuration.

If a future change makes the panel go completely silent, check this first: a
GPIO port whose clock is off fails without any diagnostic.

## Images

The panel frame format is 33 bytes per row × 176 rows = **5808 bytes**, MSB =
leftmost pixel, **1 = white**, 0 = black, top-down, no row padding.

A 1-bpp BMP holds the same pixels but with rows padded to 4 bytes
(33 → 36) and stored **bottom-up** for a positive `biHeight`. The converter
undoes both:

```bash
cd bare/eink_27in_264x176
python tools/bmp_to_eink.py ../../eink_assets/board_0.bmp \
                            ../../eink_assets/board_1.bmp \
                            --out src/resource.c
```

`src/resource.c` / `resource.h` are generated - regenerate rather than edit.
BMP palette index 0 = black / index 1 = white is assumed (the normal
monochrome BMP convention), which lets the bit values pass through unchanged
(BMP bit 1 = white = the panel's `0xFF`).

If a converted image appears **upside down** on the panel, add `--flip-y`; if
it appears as a **negative**, add `--invert`:

```bash
python tools/bmp_to_eink.py ../../eink_assets/board_0.bmp \
                            ../../eink_assets/board_1.bmp \
                            --out src/resource.c --flip-y
```

`tools/bmp_to_eink.py --help` lists the options; it is pure standard library
(no Pillow).

## Behaviour

Same loop as the original, every iteration:

```
power on panel -> clear to white (2 consecutive updates, to de-ghost)
loop:
  print the ADC buffer          (3 values, see the ADC note below)
  toggle LD2 (blue)
  full 2-stage update with board_0   -> LD1 (green) on during the update
  wait 25 s
  toggle LD2
  full 2-stage update with board_1
  wait 25 s
```

`Pv_EINK_SetTempFactor(41)` keeps the original's hard-coded 41 °C temperature
compensation. Chip temperature rises as the demo runs, so the update timing
is a little pessimistic when the board is cold - that is the original's
behaviour, not a port artefact.

### ADC note

The original configures ADC1 (temp / VREFINT / VBAT, DMA1 circular) and
prints the buffer each iteration, but leaves `HAL_ADC_Start_DMA()` commented
out - so the printed values are always `0 0 0`. That is preserved: the call is
still commented, in the same place, in `main.c`.

The init **is** ported, so uncommenting that one line gives real numbers. Two
things were fixed to make that true, which the Keil build got away with only
because the conversion never ran:

- the ADC kernel clock is configured in `MX_ADC1_Init()` (SYSCLK sync, /4 =
  30 MHz). The original relied on `SystemClock_Config()`'s PLLSAI1 setup,
  which this port bypasses in favour of the board's clock tree.
- `HAL_ADCEx_Calibration_Start()` is called. The L4 ADC needs a calibration
  before the first conversion.

## What it does *not* do

- No low-power / sleep handling: the original ran continuously, so this does
  too. An E-INK update is a blocking multi-second operation.
- No button use: the original configured B1 as an EXT1-rising input but never
  read it, so the port just leaves the board's default input alone.

## Build / flash / console

```bash
bash build.sh          # configure + build into ./build
ninja -C build flash   # probe-rs download + reset over ST-Link SWD
```

Console is **LPUART1** (PG7/PG8, ST-Link VCP, 115200 8-N-1):

```
==== nucleo-l4r5 (STM32L4R5ZIT6) eink_27in_264x176 @ 120 MHz ====
2.7" E-INK 264x176 (Pervasive Displays), SPI1 mode 0 @ 30 MHz
SCK=PA5 MISO=PA6 MOSI=PA7 CS=PD14 RST=PF15 BUSY=PE13
PWR_EN=PF14 IO_EN=PF13 BRD=PE9 DISCHARGE=PE11
console: LPUART1 PG7/PG8 @ 115200 (was ITM/SWO in the Keil build)
images: board_0, board_1 (from ../eink_assets/*.bmp)
[EINK] Pv_EINK_Init
[EINK] power on OK, clearing to white
[EINK] update -> board_0
0 0 0
[EINK] update -> board_1
...
```

A full update takes a couple of seconds and the panel flashes through the
stages - that is the E-INK refresh sequence, not a fault. Power-off leaves the
image on the panel (E-INK holds its state without power).

## Files

- `src/main.c` - demo loop, `ST_EINK_*` helpers, image table
- `src/eink_board.c` / `.h` - panel pins, SPI1 (mode 0 @ 30 MHz), ADC1/DMA,
  and `Board_Eink_Init()`
- `src/main.h` - panel pin map (names identical to the original, so the
  vendor driver stays verbatim)
- `src/pervasive_eink_hardware_driver.c` / `.h` - vendor driver, **unmodified**
- `src/pervasive_eink_configuration.h` - vendor panel/register constants,
  **unmodified**
- `src/resource.c` / `resource.h` - `board_0` / `board_1` frame images
  (generated by `tools/bmp_to_eink.py`)
- `tools/bmp_to_eink.py` - BMP → panel frame converter
