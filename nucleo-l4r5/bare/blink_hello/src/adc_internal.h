/**
  * @file    adc_internal.h
  * @brief   ADC1 internal-channel sampling for the STM32L4R5 "nucleo-l4r5" board.
  *
  * Internal channels of the master ADC1 peripheral on the STM32L4R5:
  *   - ADC1_IN0   VREFINT (internal reference voltage, ~1.21 V)
  *   - ADC1_IN17  temperature sensor
  *   - ADC1_IN18  VBAT/3 (internal 1/3 divider)
  *
  * Unlike the F4 (where the temp sensor and VBAT share IN18), the L4 puts them
  * on separate channels, so a single scan pass reads all three.
  */

#ifndef __ADC_INTERNAL_H__
#define __ADC_INTERNAL_H__

#include <stdint.h>

typedef struct {
    uint16_t raw_vrefint; /* ADC1_IN0 code  (VREFINT) */
    uint16_t raw_temp;    /* ADC1_IN17 code (temperature sensor) */
    uint16_t raw_vbat;    /* ADC1_IN18 code (VBAT/3) */
} ADC_InternalResult;

void ADC_Internal_Init(void);
void ADC_Internal_Sample(ADC_InternalResult *res);

#endif /* __ADC_INTERNAL_H__ */
