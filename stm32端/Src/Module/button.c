// button.c
#include "button.h"
#include "systick.h"
#include "main.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"

volatile uint8_t button_pressed = 0; // ����ȫ�ֱ�־

// PA11 �жϳ�ʼ��
void Button_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    // 1. ʹ�� GPIOA �� AFIO ʱ��
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    // 2. ���� PA11 Ϊ��������ģʽ
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;    // �ڲ�����
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz; // �ɼ����ٶ�����
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3. �����ⲿ�ж��� EXTI_Line11
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource11);
    EXTI_InitStruct.EXTI_Line = EXTI_Line11;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling; // �½��ش���
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    // 4. ���� NVIC �ж����ȼ�
    NVIC_InitStruct.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x0F;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x0F;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

void LED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct;

    // ʹ�� GPIOC ʱ��
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    // ���� PC13 Ϊ���������Ĭ�ϸߵ�ƽ��LED ��
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_SetBits(GPIOC, GPIO_Pin_13); // ��ʼ״̬��LED ��
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line11) != RESET)
    {
        // ������������ʹ����� Delay_ms
        Delay_ms(20);
        // ��Ȼ����Ƿ�Ϊ�͵�ƽ
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11) == Bit_RESET)
        {
            button_pressed = 1;
            GPIO_ResetBits(GPIOC, GPIO_Pin_13); // ���� LED
        }
        EXTI_ClearITPendingBit(EXTI_Line11);
    }
}

