# blink_hello — LED chase + ADC internal channels (nucleo-l4r5)

Minimal "hello" demo for the **nucleo-l4r5** board (STM32L4R5ZIT6 @ 120 MHz,
hard-float). It chases the three board LEDs and once a second samples the
**ADC1 internal channels** and prints them on the **LPUART1** console.

## What it does

- **LEDs** (high-active): LD1 green **PC7**, LD2 blue **PB7**, LD3 red **PB14** —
  one LED lit at a time, advancing every 250 ms.
- **ADC1 internal channels** (12-bit, sampled once per second):

  | Channel | What it is |
  | ------- | ---------- |
  | **ADC1_IN0**  | VREFINT (internal reference, ~1.21 V) |
  | **ADC1_IN17** | Temperature sensor |
  | **ADC1_IN18** | VBAT/3 (internal 1/3 divider) |

  VREFINT is used to back out the actual supply voltage (`Vdda`), which then
  scales the temperature and VBAT readings. The temperature uses the factory
  calibration via `TEMPSENSOR_CAL1/2` (30 °C / 130 °C, taken at Vref+ = 3.0 V);
  the calibration constants come from `stm32l4xx_ll_adc.h`.

  Unlike the F4 (where the temp sensor and VBAT share IN18), the L4 has them on
  **separate** channels — so one scan pass reads all three, no pass switching.

Example output (once per second):

```
==== nucleo-l4r5 (STM32L4R5ZIT6) blink_hello @ 120 MHz ====
SYSCLK = 120000000 Hz (120 MHz)
FLASH_ACR latency = 5 (5 WS, ICEN|DCEN|PRFTEN)
ADC1: VREFINT=1492 code, temp=943 code, VBAT=1372 code
     Vdda ~= 3317 mV, chip temp ~= 30 C, VBAT ~= 3333 mV
ADC1: VREFINT=1491 code, temp=942 code, VBAT=1371 code
     Vdda ~= 3319 mV, chip temp ~= 30 C, VBAT ~= 3333 mV
```

On a NUCLEO-144 board with no battery, `VBAT` is tied to `VDD`, so it reads
~3.3 V rather than 0.

## Build

```bash
cd nucleo-l4r5/bare/blink_hello

# GNU arm-none-eabi-gcc
bash build.sh

ninja -C build flash        # program via probe-rs (ST-Link SWD)
```

This project is GCC-only (like the F4 reference); the benchmark projects
(`../dhry_120m`, `../coremark_120m`) additionally support armclang / starm-clang.

## Console

**LPUART1** on **PG7 (TX) / PG8 (RX)**, AF8, **115200 8-N-1**, wired to the
ST-Link's virtual COM port. Read it on the `COMxx` the ST-Link enumerates.

```powershell
$sp = New-Object System.IO.Ports.SerialPort('COMxx',115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 15000; $sp.Open()
$sb = New-Object System.Text.StringBuilder
$deadline = [DateTime]::Now.AddSeconds(15)
while([DateTime]::Now -lt $deadline){ try { $b = $sp.ReadExisting(); if($b){ [void]$sb.Append($b) } else { Start-Sleep -Milliseconds 200 } } catch { break } }
$sp.Close(); $sb.ToString()
```
