#ifndef SWI2C_H
#define SWI2C_H

#include "stm32f10x.h"

#define I2C_ACK 0
#define I2C_None_ACK 1

void SWI2C_Init(uint32_t RCC_APB2Periph, GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin);
void SWI2C_Start(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin);
void SWI2C_Stop(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin);
uint8_t SWI2C_TransmitByte(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin, uint8_t Byte);
uint8_t SWI2C_ReceiveByte(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin, BitAction ACK);
#endif // __SWI2C_H
