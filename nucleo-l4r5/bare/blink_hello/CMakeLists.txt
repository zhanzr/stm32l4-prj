# LED blink + ADC internal-channel demo for the STM32L4R5ZIT6 "nucleo-l4r5" board.
# Chases the three board LEDs (LD1 PC7 / LD2 PB7 / LD3 PB14, high-active) and
# periodically samples the ADC1 internal channels (VREFINT on IN0, temperature
# sensor on IN17, VBAT/3 on IN18) and prints them over LPUART1 (PG7/PG8) at
# 115200 baud (ST-Link VCP). Runs at 120 MHz from the internal HSI.
#
# Pico-style workflow:
#   mkdir -p build && cd build
#   cmake -G Ninja ..
#   ninja           # builds .elf / .hex / .bin
#   ninja flash     # programs the board (probe-rs, ST-Link SWD)

cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(blink_hello C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/adc_internal.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32l4r5_board.cmake)
stm32l4r5_apply_board(${PROJECT_NAME}.elf "-O1")


include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
