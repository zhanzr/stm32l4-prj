/**
  * @file    gpio.h
  * @brief   Panel GPIO setup for the eink_27in_264x176 project.
  *
  * Kept as its own header because the vendor driver
  * (src/pervasive_eink_hardware_driver.c) does `#include "gpio.h"`.
  */

#ifndef __GPIO_H
#define __GPIO_H

void MX_GPIO_Init(void);

#endif /* __GPIO_H */
