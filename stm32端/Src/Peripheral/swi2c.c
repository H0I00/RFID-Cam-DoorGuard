/**
 * @file swi2c.c
 * @author ShiinaKaze (shiinakaze@qq.com)
 * @brief Software I2C interface
 * @version 0.1
 * @date 2024-05-31
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "stm32f10x.h"
#include "swi2c.h"
#include "systick.h"

#define SWI2C_Delay() Delay_us(3)

/**
 * @brief Initializes I2C peripheral
 * @param RCC_APB2Periph
 * @param GPIOx
 * @param SCL_Pin
 * @param SDA_Pin
 * @retval None
 */
void SWI2C_Init(uint32_t RCC_APB2Periph, GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = SCL_Pin | SDA_Pin;
	GPIO_Init(GPIOx, &GPIO_InitStructure);
}

/**
 * @brief  I2C start condition
 * @param GPIOx
 * @param SCL_Pin
 * @param SDA_Pin
 * @retval None
 */
void SWI2C_Start(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin)
{
	// pull up SCL,SDA for start
	GPIO_WriteBit(GPIOx, SDA_Pin, Bit_SET);
	GPIO_WriteBit(GPIOx, SCL_Pin, Bit_SET);
	SWI2C_Delay();

	// I2C start
	GPIO_WriteBit(GPIOx, SDA_Pin, Bit_RESET);
	SWI2C_Delay();
	GPIO_WriteBit(GPIOx, SCL_Pin, Bit_RESET);
	SWI2C_Delay();
}

/**
 * @brief  I2C stop condition
 * @param GPIOx
 * @param SCL_Pin
 * @param SDA_Pin
 * @retval None
 */
void SWI2C_Stop(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin)
{
	// pull down SDA for stop
	GPIO_WriteBit(GPIOx, SDA_Pin, Bit_RESET);
	SWI2C_Delay();

	// I2C stop
	GPIO_WriteBit(GPIOx, SCL_Pin, Bit_SET);
	SWI2C_Delay();
	GPIO_WriteBit(GPIOx, SDA_Pin, Bit_SET);
	SWI2C_Delay();
}

/**
 * @brief Transmit one byte using I2C
 * @param GPIOx
 * @param SCL_Pin
 * @param SDA_Pin
 * @param Byte
 * @retval ACK
 */
uint8_t SWI2C_TransmitByte(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin, uint8_t Byte)
{
	uint8_t ack = 1;
	for (uint8_t i = 0; i < 8; i++)
	{
		GPIO_WriteBit(GPIOx, SDA_Pin, (BitAction)(Byte & (0x80 >> i)));
		SWI2C_Delay();
		GPIO_WriteBit(GPIOx, SCL_Pin, Bit_SET);
		SWI2C_Delay();
		GPIO_WriteBit(GPIOx, SCL_Pin, Bit_RESET);
		SWI2C_Delay();
	}

	// receive ACK
	GPIO_WriteBit(GPIOx, SCL_Pin, Bit_SET);
	SWI2C_Delay();
	ack = GPIO_ReadInputDataBit(GPIOx, SDA_Pin);
	SWI2C_Delay();
	GPIO_WriteBit(GPIOx, SCL_Pin, Bit_RESET);
	SWI2C_Delay();
	return ack;
}

/**
 * @brief Receive one byte using I2C
 * @param GPIOx
 * @param SCL_Pin
 * @param SDA_Pin
 * @param ACK
 * @retval Byte or ACK
 */
uint8_t SWI2C_ReceiveByte(GPIO_TypeDef *GPIOx, uint16_t SCL_Pin, uint16_t SDA_Pin, BitAction ACK)
{
	uint8_t byte = 0x00;
	for (uint8_t i = 0; i < 8; i++)
	{
		GPIO_WriteBit(GPIOx, SCL_Pin, Bit_SET);
		SWI2C_Delay();
		if (GPIO_ReadInputDataBit(GPIOx, SDA_Pin) == 1)
		{
			byte |= (0x80 >> i);
		}
		SWI2C_Delay();
		GPIO_WriteBit(GPIOx, SCL_Pin, Bit_RESET);
		SWI2C_Delay();
	}
	// transmit ACK
	GPIO_WriteBit(GPIOx, SDA_Pin, ACK);
	SWI2C_Delay();
	GPIO_WriteBit(GPIOx, SCL_Pin, Bit_SET);
	SWI2C_Delay();
	GPIO_WriteBit(GPIOx, SCL_Pin, Bit_RESET);
	// release SDA
	GPIO_WriteBit(GPIOx, SDA_Pin, Bit_SET);
	SWI2C_Delay();
	return byte;
}
