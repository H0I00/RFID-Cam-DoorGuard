#include "uart.h"

void UART1_Init(uint32_t baudrate)
{
		NVIC_InitTypeDef nvic_initstruct;
    RCC_APB2PeriphClockCmd(RCC_APB_USART_GPIO | RCC_APB_USART, ENABLE);
    // GPIO
    GPIO_InitTypeDef GPIO_InitStructure;
    // UART TX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = USART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UART_GPIO, &GPIO_InitStructure);

    // UART RX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = USART_RX_PIN;
    GPIO_Init(UART_GPIO, &GPIO_InitStructure);
    // USART
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
		
		USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);									//使能接收中断
	
		nvic_initstruct.NVIC_IRQChannel = USART1_IRQn;
		nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
		nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
		nvic_initstruct.NVIC_IRQChannelSubPriority = 2;
		NVIC_Init(&nvic_initstruct);
}

void UART1_Transmit(uint8_t *TxBuffer, uint16_t TxLength)
{
    for (uint16_t i = 0; i < TxLength; i++)
    {
        /* Send one byte from USART */
        USART_SendData(USART1, TxBuffer[i]);

        /* Loop until USART DR register is empty */
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
        }
    }
}
