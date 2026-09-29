# eink_27in_264x176: Pervasive Displays 2.7" E-INK (264x176) demo on the
# NUCLEO-L4R5ZI @ 120 MHz, ported from the Keil MDK project D:\stm32l4r5-demo
# ("Eink Driver Demo for L4R5 Nucleo board", l4r5_t1).
#
# Port notes:
#   * Built with this repo's toolchains (GCC arm-none-eabi / Keil armclang /
#     ST armclang) and the shared board layer instead of the Keil project.
#   * The console is the board's LPUART1 (PG7/PG8, ST-Link VCP); the original
#     printed over ITM/SWO, and this port has no ITM dependency at all.
#   * The two demo images were replaced by the BMPs in
#     ../eink_assets/, converted at build-authoring time with
#     tools/bmp_to_eink.py into src/resource.c.
#
#   bash build.sh          # == cmake -G Ninja -S . -B build && ninja -C build
#   ninja -C build flash   # programs the board (probe-rs; ST-Link SWD)

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(eink_27in_264x176 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Board include first: it resolves STM32L4_HAL_SRC (the vendored repo-root
# drivers/) which the source list below references.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32l4r5_board.cmake)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/eink_board.c          # pin map + SPI1 + ADC1/DMA glue for this board
    src/pervasive_eink_hardware_driver.c
    src/resource.c            # board_0 / board_1 images (generated)
    # SPI for the panel; the board layer already brings in ADC/DMA/GPIO.
    ${STM32L4_HAL_SRC}/stm32l4xx_hal_spi.c
    ${STM32L4_HAL_SRC}/stm32l4xx_hal_spi_ex.c
)

stm32l4r5_apply_board(${PROJECT_NAME}.elf "-O2")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
