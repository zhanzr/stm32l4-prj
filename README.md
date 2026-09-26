# STM32L4 projects (multi-board)

Bare-metal firmware projects and tooling for multiple **STM32L4-series**
boards/chips. Each board lives in its own folder with a self-contained
`board/` (clock, LEDs, console, startup, linker script), `cmake/` (toolchain +
board helpers) and `bare/` (projects). Board-level docs — hardware, clock tree,
build/flash/console — live in each board folder's README.

## Boards

| Board          | What it is                                          |
| -------------- | --------------------------------------------------- |
| `nucleo-l4r5/` | NUCLEO-L4R5ZI (STM32L4R5ZIT6) @ 120 MHz, HSI 16 MHz, 3 LEDs (PC7/PB7/PB14), LPUART1 console @ 115200 (see its README) |

The `-<chip>` suffix in board folder names keeps it a multi-board/**multi-chip**
repo: e.g. a `nucleo-l496` board would sit next to `nucleo-l4r5`, a
`custom-l476` board would sit next to the L4R5 one, and boards with the same
MCU but a different pinout are siblings.

## Vendored HAL / CMSIS

The STM32L4 **HAL driver + CMSIS** are vendored in the repo root `drivers/`
(trimmed subset of the official `STM32Cube_FW_L4` package):

```
drivers/
├── CMSIS/
│   ├── Include/                        CMSIS core headers
│   └── Device/ST/STM32L4xx/Include/   STM32L4 device headers
└── STM32L4xx_HAL_Driver/
    ├── Inc/ (+ Legacy)                 HAL headers
    └── Src/ (all HAL modules)          HAL sources
```

Builds use this by default (`-DSTM32L4_HAL_ROOT` defaults to `../../drivers`).
The trimmed tree covers everything these projects compile; it does **not**
include the BSP, DSP, middleware, projects, docs, or `.chm` manuals.

To use the **full** official package instead (e.g. to pull in something not
vendored), point `STM32L4_HAL_ROOT` at its root when configuring:

```bash
cmake -G Ninja -DSTM32L4_HAL_ROOT="C:/Users/user1/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2" ..
```

The full package is STM32CubeMX → "Manage embedded software packages" →
STM32Cube MCU Package → "STM32Cube FW_L4 V1.18.2", installed under
`<STM32Cube>/Repository/STM32Cube_FW_L4_V1.18.2`.

## Toolchain / environment

* GNU arm-none-eabi-gcc (default) or Keil AC6 armclang (`-DSTM32_TOOLCHAIN=armclang`); ST Arm clang (starm-clang) where supported. The non-GNU compilers are located on `PATH`, so add their `bin` dirs before configuring.
* CMake + Ninja (Pico-style; MSYS2 mingw64 `build.sh` adds them to `PATH`).
* probe-rs (SWD flashing) + OpenOCD (alternative).

## Build configuration

Configure and build each project from its own directory (`<board>/bare/<project>`).
No absolute paths are needed — the build directory is a relative subfolder of
the project.

```bash
cd nucleo-l4r5/bare/dhry_120m

# default: gcc + the project's hard-coded optimization level
bash build.sh          # == cmake -G Ninja -S . -B build && ninja -C build
ninja flash            # program via probe-rs (ST-Link SWD)

# other toolchains: use one build dir per toolchain
# (CMAKE_TOOLCHAIN_FILE is cached at configure time)
BUILD_DIR=build-armclang bash build.sh -DSTM32_TOOLCHAIN=armclang
BUILD_DIR=build-starm-clang bash build.sh -DSTM32_TOOLCHAIN=starm-clang
```

> **Windows note.** `build.sh` selects the **native mingw64** CMake + Ninja on
> MSYS2. The MSYS2 (`/usr/bin`) CMake writes POSIX paths (e.g.
> `/usr/bin/cmake.exe`) into `build.ninja`, which only MSYS2-aware Ninja can
> execute — a native Ninja (MINGW64 shell or a standalone install) then fails
> with `CreateProcess failed: The system cannot find the file specified.`
> Native CMake emits plain Windows paths, so the build tree works from any
> shell. Keep CMake and Ninja from the **same** environment, and prefer
> `bash build.sh` over invoking `cmake`/`ninja` by hand.

**Optimization levels** are passed to the board-apply CMake function in each
project's `CMakeLists.txt`:

* **General projects** need nothing on the command line — their optimization is
  hard-coded at a sane level (`-O1` for simple demos, `-O2`/`-O3` for
  peripheral-heavy apps).
* **Benchmark projects** (`dhry_*`, `coremark_*`) default to aggressive flags
  and let you override them at configure time with `-DBENCH_OPT="..."` (plus
  `-DBENCH_OPT_C="..."` for C-only options). The fastest measured per-toolchain
  settings are documented in each benchmark's README. Note that armclang
  `-Omax` enables LTO (LLVM bitcode), which only Keil's armlink can link — not
  the GNU ld used here.
