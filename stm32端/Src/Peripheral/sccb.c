/**
 * @file sccb.c
 * @author ShiinaKaze (shiinakaze@qq.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-06
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "sccb.h"
#include "systick.h"

#define RCC_APB_SCCB_GPIOx RCC_APB2Periph_GPIOB
#define SCCB_GPIOx GPIOB

#define SCCB_SCL GPIO_Pin_10
#define SCCB_SDA GPIO_Pin_11
#define SCCB_ERROR 0xFF

#define SWSCCB_SCL_WriteBit(BIT_VALUE) GPIO_WriteBit(SCCB_GPIOx, SCCB_SCL, BIT_VALUE)
#define SWSCCB_SDA_WriteBit(BIT_VALUE) GPIO_WriteBit(SCCB_GPIOx, SCCB_SDA, BIT_VALUE)
#define SWSCCB_SDA_ReadBit() GPIO_ReadInputDataBit(SCCB_GPIOx, SCCB_SDA)
#define SWSCCB_Delay() Delay_us(5)

void SWSCCB_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB_SCCB_GPIOx, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = SCCB_SCL | SCCB_SDA;
	GPIO_Init(SCCB_GPIOx, &GPIO_InitStructure);
}

void SWSCCB_SDA_SetInput(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = SCCB_SDA;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SCCB_GPIOx, &GPIO_InitStructure);
}

void SWSCCB_SDA_SetOutput(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = SCCB_SDA;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SCCB_GPIOx, &GPIO_InitStructure);
}

void SWSCCB_Start(void)
{
	// make sure SIO_C SIO_D high
	SWSCCB_SCL_WriteBit(Bit_SET);
	SWSCCB_SDA_WriteBit(Bit_SET);
	SWSCCB_Delay();
	// SCCB Start condition
	SWSCCB_SDA_WriteBit(Bit_RESET);
	SWSCCB_Delay();
	SWSCCB_SCL_WriteBit(Bit_RESET);
	SWSCCB_Delay();
}

void SWSCCB_Stop(void)
{
	// make sure SIO_D high
	SWSCCB_SDA_WriteBit(Bit_RESET);
	SWSCCB_Delay();
	// SCCB Stop condition
	SWSCCB_SCL_WriteBit(Bit_SET);
	SWSCCB_Delay();
	SWSCCB_SDA_WriteBit(Bit_SET);
	SWSCCB_Delay();
}

uint8_t SWSCCB_WriteByte(uint8_t byte)
{
	uint8_t x_bit;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (byte & 0x80)
		{
			SWSCCB_SDA_WriteBit(Bit_SET);
		}
		else
		{
			SWSCCB_SDA_WriteBit(Bit_RESET);
		}
		byte <<= 1;
		SWSCCB_Delay();
		SWSCCB_SCL_WriteBit(Bit_SET);
		SWSCCB_Delay();
		SWSCCB_SCL_WriteBit(Bit_RESET);
		SWSCCB_Delay();
	}
	SWSCCB_SDA_SetInput();
	SWSCCB_Delay();
	SWSCCB_SCL_WriteBit(Bit_SET);
	x_bit = SWSCCB_SDA_ReadBit(); // X/Don't care bit, typical value is 0
	SWSCCB_Delay();
	SWSCCB_SCL_WriteBit(Bit_RESET);
	SWSCCB_Delay();
	SWSCCB_SDA_SetOutput();

	// check sccb data
	if (x_bit != 0)
	{
		return SCCB_ERROR;
	}
	return x_bit;
}

uint8_t SWSCCB_ReadByte(void)
{
	uint8_t na_bit;
	uint8_t byte = 0x00;
	SWSCCB_SDA_SetInput();
	for (uint8_t i = 0; i < 8; i++)
	{
		SWSCCB_Delay();
		SWSCCB_SCL_WriteBit(Bit_SET);
		if (SWSCCB_SDA_ReadBit())
		{
			byte |= (0x80 >> i);
		}
		SWSCCB_Delay();
		SWSCCB_SCL_WriteBit(Bit_RESET);
		SWSCCB_Delay();
	}
	SWSCCB_Delay();
	SWSCCB_SCL_WriteBit(Bit_SET);
	na_bit = SWSCCB_SDA_ReadBit(); // NA bit, typical value is 1
	SWSCCB_Delay();
	SWSCCB_SCL_WriteBit(Bit_RESET);
	SWSCCB_Delay();
	SWSCCB_SDA_SetOutput();

	// check sccb data
	if (na_bit != 1)
	{
		return SCCB_ERROR;
	}
	return byte;
}

void SWSCCB_WriteReg(uint8_t id_address, uint8_t sub_address, uint8_t data)
{
	// 3-Phase Write
	SWSCCB_Start();
	SWSCCB_WriteByte(id_address);
	SWSCCB_WriteByte(sub_address);
	SWSCCB_WriteByte(data);
	SWSCCB_Stop();
}

uint8_t SWSCCB_ReadReg(uint8_t id_address, uint8_t sub_address)
{
	uint8_t data;
	// 2-Phase Write
	SWSCCB_Start();
	SWSCCB_WriteByte(id_address);
	SWSCCB_WriteByte(sub_address);
	SWSCCB_Stop();
	// 2-Phase Read
	SWSCCB_Start();
	SWSCCB_WriteByte(id_address | 0x01);
	data = SWSCCB_ReadByte();
	SWSCCB_Stop();

	return data;
}

// void SCCB_Init(void)
// {
// 	RCC_APB2PeriphClockCmd(RCC_APB_SCCB_GPIOx, ENABLE);

// 	GPIO_InitTypeDef GPIO_InitStructure;
// 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
// 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
// 	GPIO_InitStructure.GPIO_Pin = SCCB_SCL | SCCB_SDA;
// 	GPIO_Init(SCCB_GPIOx, &GPIO_InitStructure);

// 	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);

// 	I2C_InitTypeDef I2C_InitStructure;
// 	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;								  // I2C模式
// 	I2C_InitStructure.I2C_ClockSpeed = 400000;								  // 传输速度，最大不能超过400KHZ
// 	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;						  // 时钟占空比
// 	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;								  // 主机是否设置应答
// 	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; // 主机地址位数  7位或10位
// 	I2C_InitStructure.I2C_OwnAddress1 = 0x00;								  // 主机地址（当STM32位从机时）
// 	I2C_Init(I2C2, &I2C_InitStructure);

// 	I2C_Cmd(I2C2, ENABLE);
// }

// void SCCB_WaitEvent(I2C_TypeDef *I2Cx, uint32_t I2C_EVENT)
// {
// 	uint32_t Timeout;
// 	Timeout = 10000;
// 	while (I2C_CheckEvent(I2Cx, I2C_EVENT) != SUCCESS)
// 	{
// 		Timeout--;
// 		if (Timeout == 0)
// 		{
// 			break;
// 		}
// 	}
// }

// void SCCB_WriteReg(uint8_t id_address, uint8_t sub_address, uint8_t data)
// {
// 	I2C_GenerateSTART(I2C2, ENABLE);
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT); // 检测EV5

// 	I2C_Send7bitAddress(I2C2, id_address, I2C_Direction_Transmitter); // 发送从机地址
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED); // 检测EV6（发送模式）

// 	I2C_SendData(I2C2, sub_address);						  // 发送从机寄存器地址
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTING); // 检测EV8

// 	I2C_SendData(I2C2, data);								 // 发送数据
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED); // 检测EV8_2

// 	I2C_GenerateSTOP(I2C2, ENABLE); // 停止发送
// }

// uint8_t SCCB_ReadReg(uint8_t id_address, uint8_t sub_address)
// {
// 	uint8_t data;

// 	I2C_GenerateSTART(I2C2, ENABLE);
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT);

// 	I2C_Send7bitAddress(I2C2, id_address, I2C_Direction_Transmitter);
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);

// 	I2C_SendData(I2C2, sub_address);
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED); // 此上与发送数据相同
// 	I2C_GenerateSTOP(I2C2, ENABLE);

// 	I2C_GenerateSTART(I2C2, ENABLE);
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT);

// 	I2C_Send7bitAddress(I2C2, id_address, I2C_Direction_Receiver);
// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED); // 检测EV6（接收模式）

// 	I2C_AcknowledgeConfig(I2C2, DISABLE); // 主机给从机应答位A
// 	I2C_GenerateSTOP(I2C2, ENABLE);		  // 接收数据时需要提前发送终止位

// 	SCCB_WaitEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED); // 检测EV7
// 	data = I2C_ReceiveData(I2C2);

// 	I2C_AcknowledgeConfig(I2C2, ENABLE);

// 	return data;
// }
