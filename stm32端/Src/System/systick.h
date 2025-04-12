#ifndef SYSTICK_H
#define SYSTICK_H
#include "stm32f10x.h"

void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);

void SysTick_Init(void);
uint32_t SysTick_GetCurrentTime(void);

#endif
