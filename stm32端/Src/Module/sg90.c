/* sg90.c */
#include "sg90.h"
#include "stm32f10x_tim.h"

#define PWM_PERIOD 20000  // 20ms周期（50Hz）
#define MIN_PULSE 500     // 0度对应0.5ms脉冲
#define MAX_PULSE 2500    // 180度对应2.5ms脉冲

void SG90_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_OCInitTypeDef TIM_OCInitStruct;
    
    // 1. 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);
    
    // 2. 配置PA8为复用推挽输出
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 3. 配置TIM1基础设置
    TIM_TimeBaseStruct.TIM_Prescaler = 72 - 1;  // 72MHz/72 = 1MHz
    TIM_TimeBaseStruct.TIM_Period = PWM_PERIOD - 1;  // 20ms周期
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStruct);
    
    // 4. 配置PWM模式
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_Pulse = MIN_PULSE;  // 初始角度0度
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM1, &TIM_OCInitStruct);
    
    // 5. 使能预装载和定时器
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);  // 高级定时器需要使能主输出
}

void SG90_SetAngle(uint8_t angle) {
    // 将角度转换为脉冲宽度（0~180度对应500~2500us）
    uint16_t pulse = MIN_PULSE + (angle * (MAX_PULSE - MIN_PULSE)) / 180;
    TIM_SetCompare1(TIM1, pulse);
}
