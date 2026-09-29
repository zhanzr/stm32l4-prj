/**
  * @file    dma.h
  * @brief   DMA setup for the eink_27in_264x176 project.
  *
  * Kept as its own header because the vendor driver
  * (src/pervasive_eink_hardware_driver.c) does `#include "dma.h"`.
  *
  * Only the ADC1 channel is used: the panel bus runs in blocking SPI mode,
  * so the original project's SPI1 TX/RX DMA streams are not ported.
  */

#ifndef __DMA_H
#define __DMA_H

#include "stm32l4xx_hal.h"

extern DMA_HandleTypeDef hdma_adc1;

void MX_DMA_Init(void);

#endif /* __DMA_H */
