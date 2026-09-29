/**
  * @file    spi.h
  * @brief   SPI1 (the panel bus) for the eink_27in_264x176 project.
  *
  * Kept as its own header because the vendor driver
  * (src/pervasive_eink_hardware_driver.c) does `#include "spi.h"` and
  * references `hspi1`; the driver file itself stays vendor-verbatim.
  */

#ifndef __SPI_H
#define __SPI_H

#include "stm32l4xx_hal.h"

extern SPI_HandleTypeDef hspi1;

void MX_SPI1_Init(void);

#endif /* __SPI_H */
