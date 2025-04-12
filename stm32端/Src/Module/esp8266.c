/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	esp8266.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2017-05-08
	*
	*	版本： 		V1.0
	*
	*	说明： 		ESP8266的简单驱动
	*
	*	修改记录：	
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "stm32f10x.h"

//网络设备驱动
#include "esp8266.h"

//硬件驱动
//#include "delay.h"
#include "uart.h"
#include "OLED.h"
#include "systick.h"
#include "main.h"
#include "sg90.h"

//C库
#include <string.h>
#include <stdio.h>


#define ESP8266_WIFI_INFO		"AT+CWJAP=\"Test\",\"test12345\"\r\n"
#define WINDOWS_IP    "AT+CIPSTART=\"TCP\",\"192.168.203.60\",8089\r\n"

unsigned char esp8266_buf[512];
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;

//延时系数
unsigned char UsCount = 0;
unsigned short MsCount = 0;



/*
************************************************************
*	函数名称：	Delay_Init
*
*	函数功能：	systick初始化
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
************************************************************
*/
void Delay_Init(void)
{

	SysTick->CTRL &= ~(1 << 2);		//选择时钟为HCLK(72MHz)/8		103--9MHz
	
	UsCount = 9;					//微秒级延时系数
	
	MsCount = UsCount * 1000;		//毫秒级延时系数

}

/*
************************************************************
*	函数名称：	DelayUs
*
*	函数功能：	微秒级延时
*
*	入口参数：	us：延时的时长
*
*	返回参数：	无
*
*	说明：		此时钟(21MHz)最大延时798915us
************************************************************
*/
void DelayUs(unsigned short us)
{

	unsigned int ctrlResult = 0;
	
	us &= 0x00FFFFFF;											//取低24位
	
	SysTick->LOAD = us * UsCount;								//装载数据
	SysTick->VAL = 0;
	SysTick->CTRL = 1;											//使能倒计数器
	
	do
	{
		ctrlResult = SysTick->CTRL;
	}
	while((ctrlResult & 0x01) && !(ctrlResult & (1 << 16)));	//保证在运行、检查是否倒计数到0
	
	SysTick->CTRL = 0;											//关闭倒计数器
	SysTick->VAL = 0;

}

/*
************************************************************
*	函数名称：	DelayXms
*
*	函数功能：	毫秒级延时
*
*	入口参数：	ms：延时的时长
*
*	返回参数：	无
*
*	说明：		
************************************************************
*/
void DelayXms(unsigned short ms)
{

	unsigned int ctrlResult = 0;
	
	if(ms == 0)
		return;
	
	ms &= 0x00FFFFFF;											//取低24位
	
	SysTick->LOAD = ms * MsCount;								//装载数据
	SysTick->VAL = 0;
	SysTick->CTRL = 1;											//使能倒计数器
	
	do
	{
		ctrlResult = SysTick->CTRL;
	}
	while((ctrlResult & 0x01) && !(ctrlResult & (1 << 16)));	//保证在运行、检查是否倒计数到0
	
	SysTick->CTRL = 0;											//关闭倒计数器
	SysTick->VAL = 0;

}

/*
************************************************************
*	函数名称：	DelayMs
*
*	函数功能：	微秒级长延时
*
*	入口参数：	ms：延时的时长
*
*	返回参数：	无
*
*	说明：		多次调用DelayXms，做到长延时
************************************************************
*/
void DelayMs(unsigned short ms)
{

	unsigned char repeat = 0;
	unsigned short remain = 0;
	
	repeat = ms / 500;
	remain = ms % 500;
	
	while(repeat)
	{
		DelayXms(500);
		repeat--;
	}
	
	if(remain)
		DelayXms(remain);

}

void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len)
{

	unsigned short count = 0;
	
	for(; count < len; count++)
	{
		USART_SendData(USARTx, *str++);									//发送数据
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);		//等待发送完成
	}

}


//==========================================================
//	函数名称：	ESP8266_Clear
//
//	函数功能：	清空缓存
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_Clear(void)
{

	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;

}

//==========================================================
//	函数名称：	ESP8266_WaitRecive
//
//	函数功能：	等待接收完成
//
//	入口参数：	无
//
//	返回参数：	REV_OK-接收完成		REV_WAIT-接收超时未完成
//
//	说明：		循环调用检测是否接收完成
//==========================================================
_Bool ESP8266_WaitRecive(void)
{

	if(esp8266_cnt == 0) 							//如果接收计数为0 则说明没有处于接收数据中，所以直接跳出，结束函数
		return REV_WAIT;
		
	if(esp8266_cnt == esp8266_cntPre)				//如果上一次的值和这次相同，则说明接收完毕
	{
		esp8266_cnt = 0;							//清0接收计数
			
		return REV_OK;								//返回接收完成标志
	}
		
	esp8266_cntPre = esp8266_cnt;					//置为相同
	
	return REV_WAIT;								//返回接收未完成标志

}

//==========================================================
//	函数名称：	ESP8266_SendCmd
//
//	函数功能：	发送命令
//
//	入口参数：	cmd：命令
//				res：需要检查的返回指令
//
//	返回参数：	0-成功	1-失败
//
//	说明：		
//==========================================================
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
	
	unsigned char timeOut = 200;

	Usart_SendString(USART1, (unsigned char *)cmd, strlen((const char *)cmd));
	
	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)							//如果收到数据
		{
			if(strstr((const char *)esp8266_buf, res) != NULL)		//如果检索到关键词
			{
				ESP8266_Clear();									//清空缓存
				
				return 0;
			}
		}
		
		DelayXms(10);
	}
	
	return 1;

}

//==========================================================
//	函数名称：	ESP8266_SendData
//
//	函数功能：	发送数据
//
//	入口参数：	data：数据
//				len：长度
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_SendData(unsigned char *data, unsigned short len)
{

	char cmdBuf[32];
	
	ESP8266_Clear();								//清空接收缓存
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);		//发送命令
	if(!ESP8266_SendCmd(cmdBuf, ">"))				//收到‘>’时可以发送数据
	{
		Usart_SendString(USART1, data, len);		//发送设备连接请求数据
	}

}

//==========================================================
//	函数名称：	ESP8266_GetIPD
//
//	函数功能：	获取平台返回的数据
//
//	入口参数：	等待的时间(乘以10ms)
//
//	返回参数：	平台返回的原始数据
//
//	说明：		不同网络设备返回的格式不同，需要去调试
//				如ESP8266的返回格式为	"+IPD,x:yyy"	x代表数据长度，yyy是数据内容
//==========================================================
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{

	char *ptrIPD = NULL;
	
	do
	{
		if(ESP8266_WaitRecive() == REV_OK)								//如果接收完成
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");				//搜索“IPD”头
			if(ptrIPD == NULL)											//如果没找到，可能是IPD头的延迟，还是需要等待一会，但不会超过设定的时间
			{
				//UsartPrintf(USART_DEBUG, "\"IPD\" not found\r\n");
			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');							//找到':'
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;
				
			}
		}
		
		DelayXms(5);													//延时等待
	} while(timeOut--);
	
	return NULL;														//超时还未找到，返回空指针

}

//==========================================================
//	函数名称：	ESP8266_Init
//
//	函数功能：	初始化ESP8266
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_Init(void)
{
	Delay_Init();
	ESP8266_Clear();
//AT+RESTORE                                       #恢复出厂设置
//AT+CWMODE=1                                      #设置ESP8266工作模式为STA
//AT+RST                                           #复位
//AT+CWJAP="路由器账号","密码"                     #连接路由器
//AT+CIPMODE=1                                     #设置透传模式
//AT+CIPSTART="TCP","192.168.1.11",8266           #连接TCP服务器（上位机）
//AT+CIPSEND                                       #开启透传
	//while(ESP8266_SendCmd("AT+RST\r\n", "OK"))
		//DelayXms(500);
		
//	UsartPrintf(USART_DEBUG, "1. AT\r\n");
//	OLED_Clear(); OLED_ShowString(0,0,"1.AT...",8);
	while(ESP8266_SendCmd("AT\r\n", "OK"))
		DelayXms(500);
	
//	UsartPrintf(USART_DEBUG, "2. CWMODE\r\n");
//	OLED_ShowString(0,2,"2.CWMODE...",8);
	while(ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
		DelayXms(500);
	
//	UsartPrintf(USART_DEBUG, "3. AT+CWDHCP\r\n");
//	OLED_ShowString(0,4,"3.AT+CWDHCP...",8);
	while(ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK"))
		DelayXms(500);
	
//	UsartPrintf(USART_DEBUG, "4. CWJAP\r\n");
//	OLED_ShowString(0,6,"4.CWJAP...",8);
	while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
		DelayXms(500);
	//#define ESP8266_WIFI_INFO		"AT+CWJAP=\"Test\",\"test12345\"\r\n"
	
	while(ESP8266_SendCmd("AT+CIPMODE=0\r\n", "OK"))
		DelayXms(500);

	//UsartPrintf(USART_DEBUG, "5. ESP8266 Init OK\r\n");
	//OLED_Clear(); OLED_ShowString(0,0,"ESP8266 Init OK",16); DelayXms(500);
	//while(ESP8266_SendCmd("AT+CWJAP?\r\n", "OK"))
		//DelayXms(500);
	//while(ESP8266_SendCmd("AT+CIFSR\r\n", "OK"))
		//DelayXms(500);
	while(ESP8266_SendCmd(WINDOWS_IP, "CONNECT"))
		DelayXms(500);
	//#define WINDOWS_IP    "AT+CIPSTART=\"TCP\",\"172.20.10.6\",8089\r\n"
}

//==========================================================
//	函数名称：	USART_IRQHandler
//
//	函数功能：	串口2收发中断
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void USART1_IRQHandler(void)
{

	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) //接收中断
	{
		if(esp8266_cnt >= sizeof(esp8266_buf))	esp8266_cnt = 0; //防止串口被刷爆
		esp8266_buf[esp8266_cnt++] = USART1->DR;
		
		USART_ClearFlag(USART1, USART_FLAG_RXNE);
	}

}


// 新增全局变量和命令检查函数
volatile uint8_t esp8266_cmd_flag = 0;




void ESP8266_CheckCommand(void) {

    char *ptr = strstr((char*)esp8266_buf, "pic");

    if (ptr != NULL) {

        esp8266_cmd_flag = 1;

        ESP8266_Clear();

    }

    // ??????????

    else if(strstr((char*)esp8266_buf, "OPEN_DOOR")) {

        // ???????????

        ESP8266_Clear();

    }

}
