# Vendored STM32L4 HAL + CMSIS

Trimmed subset of the official **STM32Cube_FW_L4** package (V1.18.2), vendored
so every board in this repo can build without an STM32CubeMX install.

```
drivers/
├── CMSIS/
│   ├── Include/                         CMSIS core headers
│   └── Device/ST/STM32L4xx/
│       ├── Include/                     STM32L4 device headers (stm32l4xx.h, ...)
│       ├── Source/                      (template system_stm32l4xx.c)
│       ├── LICENSE.txt / License.md
└── STM32L4xx_HAL_Driver/
    ├── Inc/ (+ Legacy/)                 HAL headers
    └── Src/                             HAL sources
```

Consumed by each board's `cmake/stm32l4r5_board.cmake` via `STM32L4_HAL_ROOT`
(default `../../drivers`). Only the modules a board's projects actually compile
are attached to the target, so unused HAL sources are simply never built.

The vendored tree is a trimmed subset: besides the core/RCC/GPIO/UART modules it
includes the **ADC** HAL (`stm32l4xx_hal_adc.c` / `_ex.c`, plus
`stm32l4xx_ll_adc.h`) used by `nucleo-l4r5/bare/blink_hello` for the internal
channels (VREFINT / temperature / VBAT), and the **SPI** and **I2C** HAL
(`stm32l4xx_hal_spi.c` / `_ex.c`, `stm32l4xx_hal_i2c.c` / `_ex.c`) used by
`nucleo-l4r5/bare/st7789s_md120_240x240_ft6336` (LCD over SPI1, touch over I2C1).

To use the **full** official package instead, configure with:

```bash
cmake -G Ninja -DSTM32L4_HAL_ROOT="C:/path/to/STM32Cube_FW_L4_V1.18.2" ..
```
