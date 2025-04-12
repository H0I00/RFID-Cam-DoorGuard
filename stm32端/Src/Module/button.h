// button.h
#ifndef __BUTTON_H
#define __BUTTON_H

#include "stm32f10x.h"

void Button_Init(void);                // 初始化函数
void LED_Init(void);                // 初始化函数
extern volatile uint8_t button_pressed; // 全局按钮按下标志

#endif

