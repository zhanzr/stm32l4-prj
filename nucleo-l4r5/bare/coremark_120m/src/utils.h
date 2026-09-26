#ifndef __UTILS_H__
#define	__UTILS_H__

#include <stdint.h>
#include "stm32l4xx_hal.h"

#define RAM_FUNC

#define PRINTF(...)  printf(__VA_ARGS__)

void TICK_Init(void);

#endif
