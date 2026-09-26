# Dhrystone 2.1 @ 120 MHz — nucleo-l4r5 (STM32L4R5ZIT6)

Classic Dhrystone 2.1 (`dhry_1.c` / `dhry_2.c` / `dhry.h`), **10,000,000
runs**, on the **nucleo-l4r5** board (STM32L4R5ZIT6) clocked at **120 MHz**
(HSI 16 MHz, PLL M=2 N=30 R=2 → SYSCLK, hard-float). Compiler-agnostic: the
same sources build with either **GNU arm-none-eabi-gcc** or **Keil Arm Compiler
6 (armclang)**, selected at configure time. Timing uses the SysTick
(`MSC_CLOCK` / `HAL_GetTick`) 1 ms tick, and results are printed over LPUART1
(PG7/PG8 → ST-Link VCP, 115200 8-N-1).

## Results

Measured on hardware at 120 MHz (hard-float): capture the console while the
chip runs the benchmark (it re-runs every ~51 s), and take the last complete
`Dhrystones per Second` line.

| Toolchain           | Flags                                      | µs/run | Dhrystones/s | DMIPS/MHz |
| ------------------- | ------------------------------------------ | ------ | ------------ | --------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-loops` | 4.143  | **241,365**  | **1.145** |
| ARMCLANG (Keil AC6) | `-Ofast -ffp-contract=fast -funroll-loops` | 3.576  | **279,642**  | **1.326** |

All runs print correct final values (Int_Glob=5, Bool_Glob=1, Ch_1_Glob='A',
Ch_2_Glob='B', Arr_1_Glob[8]=7, Arr_2_Glob[8][7]=runs+10).

> ⚠ **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists the loop-invariant work out of the timed loop, inflating the score
> ~2.2× (to 526,177 Dhrystones/s on this board) while still passing the
> final-value check. The LTO number is meaningless and is **excluded from the
> comparison above**. Full explanation and reproduction:
> **`LTO_on_dhrystone.md`** in this folder.

## Most aggressive flags

- **ARMCLANG (Keil AC6): `-Omax` gives nothing over `-Ofast`** for Dhrystone —
  both measure 279,642 Dhrystones/s (3.576 µs/run). The timed loop is already
  fully optimized, so use the default `-Ofast -ffp-contract=fast
  -funroll-loops`. (`-Omax` additionally needs `-fno-lto` in this repo, since
  bare `-Omax` emits LLVM LTO objects that GNU ld cannot link.)
- **GCC:** `-Ofast -ffp-contract=fast -funroll-loops` (the default) →
  241,365 Dhrystones/s; **do not** add `-flto` (inflates to 526,177 — an
  artifact, see `LTO_on_dhrystone.md`).
- **ST Arm clang:** not supported for Dhrystone in this repo (gcc/armclang
  only — see `CMakeLists.txt`).

```bash
BUILD_DIR=build-armclang bash build.sh -DSTM32_TOOLCHAIN=armclang
BUILD_DIR=build-gcc      bash build.sh -DSTM32_TOOLCHAIN=gcc
```

## Build

Requires the CMake/Ninja environment from the board-level `../../README.md`.

```bash
# GNU gcc (default)
bash build.sh
ninja -C build flash          # programs the board via probe-rs / ST-Link (SWD)

# armclang (optional) — put Keil's bin dir on PATH so CMake can find armclang
export PATH="/d/Keil_v5/ARM/ARMCLANG/bin:$PATH"
BUILD_DIR=build-armclang bash build.sh -DSTM32_TOOLCHAIN=armclang

# GNU gcc + LTO (kept only as reproducible evidence of the artifact — see LTO_on_dhrystone.md)
BUILD_DIR=build-gcc-lto bash build.sh -DSTM32_LTO=ON
```

Use a separate build dir per toolchain (`build/`, `build-gcc-lto/`,
`build-armclang/`) because `CMAKE_TOOLCHAIN_FILE` is cached after configure.

> `build.sh` selects the native mingw64 CMake + Ninja — see the Windows note in
> the root `README.md`.

## Why the armclang printf shim?

Keil's armclang runs in ARMCLIB "standardlib" mode and specializes calls to
the *name* `printf` into the ARMCLIB ABI (`__2printf` + hidden `_printf_*`
helpers). Those symbols only exist in ARMCLIB, so linking against GNU
newlib with GNU ld fails. `../../cmake/printf_rename.h` renames `printf` →
`bench_printf` (a `vprintf` wrapper in `../../board/uart_printf.c`); armclang
does not specialize `vprintf`, so all console output still reaches the UART.

## Console

Results are printed over the board's console **LPUART1** on **PG7 (TX) / PG8
(RX)**, AF8, **115200 8-N-1**, wired to the ST-Link's virtual COM port (see the
board-level `../../README.md` for the port/baud and a capture recipe).
