# AGENTS.md

Guidance for AI agents (and humans) working in this repo. Keep it brief; the
authoritative docs already live in each board's `README.md` and the root
`README.md`.

## What this repo is

Bare-metal firmware for **STM32L4-series** boards. It is a **multi-board /
multi-chip** repo: each board lives in its own `<name>-<chip>` folder
(`nucleo-l4r5`) and is fully self-contained:

- `board/` — clock init, LEDs, console, startup assembly, linker script,
  `stm32l4xx_hal_conf.h`, `stm32l4xx_hal_msp.c`, `stm32l4xx_it.c`, newlib stubs.
- `cmake/` — toolchain files, the board-apply helper, flash targets, OpenOCD cfg.
- `bare/` — the actual projects; each has a `build.sh`, a `CMakeLists.txt`, and a
  `src/` directory.
- Vendored HAL/CMSIS lives in the **repo root** `drivers/` (all boards share it).

Per-board hardware/clock/console facts differ — **always read the board's own
`README.md`** before touching or writing a project for it (e.g. the L4R5 board
runs from the internal HSI at 120 MHz with the console on LPUART1 PG7/PG8, not
an external crystal on USART1).

## Build & flash workflow

Pico-style **CMake + Ninja**, run per-project. From a project dir
(`bare/dhry_120m`, etc.):

```bash
bash build.sh                     # configure + build into ./build
ninja flash                       # probe-rs download + reset over SWD
ninja flash-reset                 # same, but --connect-under-reset (stuck/low-power target)
ninja flash-ocd                   # alternative: flash via OpenOCD

# extra toolchains / configs: one build dir each (CMAKE_TOOLCHAIN_FILE is cached)
BUILD_DIR=build-armclang bash build.sh -DSTM32_TOOLCHAIN=armclang
```

The `build/` directories under each project are **generated** (and git-ignored);
do not hand-edit them.

- Toolchain: GNU arm-none-eabi-gcc (default) **or** Keil AC6 `armclang`
  (`-DSTM32_TOOLCHAIN=armclang`) **or** ST Arm clang
  (`-DSTM32_TOOLCHAIN=starm-clang`). `build.sh` puts the native CMake/Ninja on
  `PATH`; for armclang / starm-clang, add their `bin` dirs to `PATH` first (the
  toolchain files locate them with `find_program`).
- **CMake/Ninja environment matters on Windows.** `build.sh` prefers the
  **native mingw64** CMake + Ninja. MSYS2's `/usr/bin` CMake writes POSIX paths
  (`/usr/bin/cmake.exe`) into `build.ninja`, which only MSYS2-aware Ninja can
  run; a native Ninja then fails with `CreateProcess failed: The system cannot
  find the file specified.` Never mix the two — regenerate with `bash build.sh`
  (or delete the `build*/` dir) if CMake and Ninja come from different sides.
- Flash uses **probe-rs** by default (auto-detects the probe); OpenOCD is the
  alternative. Pin a specific probe with `-DDEBUG_PROBE=<selector>` and set the
  SWD clock with `-DDEBUG_SPEED=<kHz>` at configure time.
- `VAPP is 0.00 V` / `JtagGetIdcodeError` from probe-rs means the **target is
  unpowered** (IDD/VDD jumpers, PWR LED, cable/port) — a board problem, not a
  build one. See the Troubleshooting section in `nucleo-l4r5/README.md`.

## Project structure conventions

When adding a new project, follow the pattern of an existing one
(e.g. `nucleo-l4r5/bare/dhry_120m`):

- `CMakeLists.txt`:
  - `cmake_minimum_required(VERSION 3.13)`; set `CMAKE_TOOLCHAIN_FILE` to
    `../../cmake/arm-none-eabi-toolchain.cmake` if not already set.
  - `project(<name> C ASM)` — **ASM is required** (startup `.s`).
  - `add_executable(<name>.elf src/... )`.
  - `include(../../cmake/stm32l4r5_board.cmake)` and call the board-apply
    function with the optimization flag, e.g.
    `stm32l4r5_apply_board(${PROJECT_NAME}.elf "-O1")`.
  - Add the objcopy `.hex`/`.bin` POST_BUILD command and
    `include(../../cmake/flash-targets.cmake)`.
- `build.sh`: copy the existing one verbatim (it is generic).
- `README.md`: document hardware, build, flash, console.

Board code lives in `board/`, **not** per-project. New shared helper source
belongs there and is pulled in by the board-apply CMake function.

## Conventions & gotchas

- HAL vendor tree is shared across boards in root `drivers/`; `STM32L4_HAL_ROOT`
  defaults to `../../drivers`. To use the full STM32Cube_FW_L4 package instead,
  pass `-DSTM32L4_HAL_ROOT` at configure time.
- Compile definitions set by the board layer: `STM32L4R5xx` and `USE_HAL_DRIVER`.
- `-ffunction-sections -fdata-sections` + `--gc-sections` are on by default;
  benchmarks add aggressive flags like `-Ofast -ffp-contract=fast -funroll-loops`.
- **LTO** (`-DSTM32_LTO=ON`) is GCC-only; armclang ignores it (LLVM bitcode
  cannot be consumed by GNU ld). When LTO is on, `syscalls.c` is forcibly
  compiled `-fno-lto` — do not remove that.
- Console output goes through the board `UART_PutChar` (LPUART1) via `_write`,
  not `printf` directly. Do not add a second retarget.
- Do not add code comments beyond what's needed; the existing sources use
  sparse explanatory comments only where non-obvious (e.g. LTO/syscalls workaround).
- There is no test suite in this repo; verification is by building + flashing
  a target board (or just `bash build.sh` to confirm it compiles clean).
