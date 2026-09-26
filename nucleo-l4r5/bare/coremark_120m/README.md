# CoreMark 1.0.1 @ 120 MHz — nucleo-l4r5 (STM32L4R5ZIT6)

CoreMark 1.0.1 (EEMBC, `coremark_1_0_1/`), **10,000 iterations**, on the
**nucleo-l4r5** board (STM32L4R5ZIT6) at **120 MHz** (HSI 16 MHz → PLL M=2
N=30 R=2 → SYSCLK, hard-float). Compiler-agnostic: the same sources build with
**GNU arm-none-eabi-gcc**, **Keil Arm Compiler 6 (armclang)** or **ST Arm
clang** (starm-clang), selected at configure time. The CoreMark port uses the
HAL SysTick 1 kHz tick (`clock()`/`usec()`) from `src/core_portme.c`, so it
works identically on all three compilers.

## Results

Measured on hardware at 120 MHz (hard-float): capture the console while the
chip runs the benchmark (it re-runs every ~45 s), and take the last complete
`Iterations/Sec` line.

| Toolchain           | Flags                                          | iterations/s | Time (s) |
| ------------------- | ---------------------------------------------- | ------------ | -------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-all-loops` | 289.97       | 34.49    |
| GCC 15.3.1 + LTO    | above `+ -flto`                                | 272.24       | 36.73    |
| ARMCLANG (Keil AC6) | `-Ofast -ffp-contract=fast -funroll-all-loops` | 293.83       | 34.03    |
| ARMCLANG (Keil AC6) | `-Omax -fno-lto`                               | **339.94**   | **29.42** |
| ST Arm clang 21.1.1 | `-Ofast -ffp-contract=fast`                    | 263.16       | 38.00    |

All runs share the same CRC (`crcfinal 0x988c`) and report
`Correct operation validated`. Note CoreMark's per-run CRC forces the work to
execute, so **LTO does not inflate it** the way it cheats Dhrystone — see
`../dhry_120m/LTO_on_dhrystone.md`.

## Most aggressive flags

Highest measured score per toolchain (see Results):

- **ARMCLANG (Keil AC6): `-Omax -fno-lto`** — 339.94 it/s. Bare `-Omax` makes
  armclang emit LLVM **LTO** objects that GNU ld cannot link, hence `-fno-lto`.
  Pass it as a **C-only** flag (`BENCH_OPT_C`) so it stays off the asm/link
  steps, with `BENCH_OPT` cleared.
- **GCC:** `-Ofast -ffp-contract=fast -funroll-all-loops` (already the default)
  → 289.97 it/s; adding `-DSTM32_LTO=ON` does **not** help here (272.24 it/s).
- **ST Arm clang:** flat at its `-Ofast -ffp-contract=fast` default (263.16
  it/s); `-funroll-all-loops` is not supported by clang.

```bash
BUILD_DIR=build-armclang-omax bash build.sh -DSTM32_TOOLCHAIN=armclang '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto'
BUILD_DIR=build-gcc-lto       bash build.sh -DSTM32_LTO=ON
BUILD_DIR=build-starm-clang   bash build.sh -DSTM32_TOOLCHAIN=starm-clang
```

## Build

Requires the CMake/Ninja environment from the board-level `../../README.md`.

```bash
cd nucleo-l4r5/bare/coremark_120m

# GNU gcc (default)
bash build.sh
ninja -C build flash          # programs the board via probe-rs / ST-Link (SWD)

# armclang (optional) — put Keil's bin dir on PATH so CMake can find armclang
export PATH="/d/Keil_v5/ARM/ARMCLANG/bin:$PATH"
BUILD_DIR=build-armclang bash build.sh -DSTM32_TOOLCHAIN=armclang

# armclang at -Omax (BENCH_OPT_C is applied to the C files only, and -fno-lto
# is required because -Omax would otherwise emit LTO objects GNU ld cannot link)
BUILD_DIR=build-armclang-omax bash build.sh -DSTM32_TOOLCHAIN=armclang \
    '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto'

# GNU gcc + LTO (valid for CoreMark, but does not help here)
BUILD_DIR=build-gcc-lto bash build.sh -DSTM32_LTO=ON

# ST Arm clang (starm-clang) + LLD — add its bin dir to PATH first
export PATH="/d/ST/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.llvm.win32_1.0.200.202603311046/tools/bin:$PATH"
BUILD_DIR=build-starm-clang bash build.sh -DSTM32_TOOLCHAIN=starm-clang
BUILD_DIR=build-starm-lto   bash build.sh -DSTM32_TOOLCHAIN=starm-clang -DSTM32_LTO=ON
```

`build.sh` puts the native mingw64 CMake/Ninja on `PATH`; for armclang /
starm-clang, add their `bin` dirs to `PATH` first (the toolchain files locate
them with `find_program`). See the Windows note in the root `README.md`.

All configurations above (gcc, gcc+LTO, armclang, armclang `-Omax -fno-lto`,
starm-clang, starm-clang+LTO) have been verified to build with this repo's
board layer.

Use a separate build dir per toolchain (`build/`, `build-gcc-lto/`,
`build-armclang/`, `build-starm-clang/`) because `CMAKE_TOOLCHAIN_FILE` is
cached after configure.

## Console

**LPUART1** on **PG7 (TX) / PG8 (RX)**, AF8, **115200 8-N-1**, wired to the
ST-Link's virtual COM port (see the board-level `../../README.md` for a capture
recipe).
