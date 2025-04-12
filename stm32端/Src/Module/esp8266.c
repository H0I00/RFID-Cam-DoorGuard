/**
	************************************************************
	************************************************************
	************************************************************
	*	�ļ����� 	esp8266.c
	*
	*	���ߣ� 		�ż���
	*
	*	���ڣ� 		2017-05-08
	*
	*	�汾�� 		V1.0
	*
	*	˵���� 		ESP8266�ļ�����
	*
	*	�޸ļ�¼��	
	************************************************************
	************************************************************
	************************************************************
**/

//��Ƭ��ͷ�ļ�
#include "stm32f10x.h"

//�����豸����
#include "esp8266.h"

//Ӳ������
//#include "delay.h"
#include "uart.h"
#include "OLED.h"
#include "systick.h"
#include "main.h"
#include "sg90.h"

//C��
#include <string.h>
#include <stdio.h>


#define ESP8266_WIFI_INFO		"AT+CWJAP=\"Test\",\"test12345\"\r\n"
#define WINDOWS_IP    "AT+CIPSTART=\"TCP\",\"192.168.203.60\",8089\r\n"

unsigned char esp8266_buf[512];
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;

//��ʱϵ��
unsigned char UsCount = 0;
unsigned short MsCount = 0;



/*
************************************************************
*	�������ƣ�	Delay_Init
*
*	�������ܣ�	systick��ʼ��
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
************************************************************
*/
void Delay_Init(void)
{

	SysTick->CTRL &= ~(1 << 2);		//ѡ��ʱ��ΪHCLK(72MHz)/8		103--9MHz
	
	UsCount = 9;					//΢�뼶��ʱϵ��
	
	MsCount = UsCount * 1000;		//���뼶��ʱϵ��

}

/*
************************************************************
*	�������ƣ�	DelayUs
*
*	�������ܣ�	΢�뼶��ʱ
*
*	��ڲ�����	us����ʱ��ʱ��
*
*	���ز�����	��
*
*	˵����		��ʱ��(21MHz)�����ʱ798915us
************************************************************
*/
void DelayUs(unsigned short us)
{

	unsigned int ctrlResult = 0;
	
	us &= 0x00FFFFFF;											//ȡ��24λ
	
	SysTick->LOAD = us * UsCount;								//װ������
	SysTick->VAL = 0;
	SysTick->CTRL = 1;											//ʹ�ܵ�������
	
	do
	{
		ctrlResult = SysTick->CTRL;
	}
	while((ctrlResult & 0x01) && !(ctrlResult & (1 << 16)));	//��֤�����С�����Ƿ񵹼�����0
	
	SysTick->CTRL = 0;											//�رյ�������
	SysTick->VAL = 0;

}

/*
************************************************************
*	�������ƣ�	DelayXms
*
*	�������ܣ�	���뼶��ʱ
*
*	��ڲ�����	ms����ʱ��ʱ��
*
*	���ز�����	��
*
*	˵����		
************************************************************
*/
void DelayXms(unsigned short ms)
{

	unsigned int ctrlResult = 0;
	
	if(ms == 0)
		return;
	
	ms &= 0x00FFFFFF;											//ȡ��24λ
	
	SysTick->LOAD = ms * MsCount;								//װ������
	SysTick->VAL = 0;
	SysTick->CTRL = 1;											//ʹ�ܵ�������
	
	do
	{
		ctrlResult = SysTick->CTRL;
	}
	while((ctrlResult & 0x01) && !(ctrlResult & (1 << 16)));	//��֤�����С�����Ƿ񵹼�����0
	
	SysTick->CTRL = 0;											//�رյ�������
	SysTick->VAL = 0;

}

/*
************************************************************
*	�������ƣ�	DelayMs
*
*	�������ܣ�	΢�뼶����ʱ
*
*	��ڲ�����	ms����ʱ��ʱ��
*
*	���ز�����	��
*
*	˵����		��ε���DelayXms����������ʱ
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
		USART_SendData(USARTx, *str++);									//��������
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);		//�ȴ��������
	}

}


//==========================================================
//	�������ƣ�	ESP8266_Clear
//
//	�������ܣ�	��ջ���
//
//	��ڲ�����	��
//
//	���ز�����	��
//
//	˵����		
//==========================================================
void ESP8266_Clear(void)
{

	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;

}

//==========================================================
//	�������ƣ�	ESP8266_WaitRecive
//
//	�������ܣ�	�ȴ��������
//
//	��ڲ�����	��
//
//	���ز�����	REV_OK-�������		REV_WAIT-���ճ�ʱδ���
//
//	˵����		ѭ�����ü���Ƿ�������
//==========================================================
_Bool ESP8266_WaitRecive(void)
{

	if(esp8266_cnt == 0) 							//������ռ���Ϊ0 ��˵��û�д��ڽ��������У�����ֱ����������������
		return REV_WAIT;
		
	if(esp8266_cnt == esp8266_cntPre)				//�����һ�ε�ֵ�������ͬ����˵���������
	{
		esp8266_cnt = 0;							//��0���ռ���
			
		return REV_OK;								//���ؽ�����ɱ�־
	}
		
	esp8266_cntPre = esp8266_cnt;					//��Ϊ��ͬ
	
	return REV_WAIT;								//���ؽ���δ��ɱ�־

}

//==========================================================
//	�������ƣ�	ESP8266_SendCmd
//
//	�������ܣ�	��������
//
//	��ڲ�����	cmd������
//				res����Ҫ���ķ���ָ��
//
//	���ز�����	0-�ɹ�	1-ʧ��
//
//	˵����		
//==========================================================
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
	
	unsigned char timeOut = 200;

	Usart_SendString(USART1, (unsigned char *)cmd, strlen((const char *)cmd));
	
	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)							//����յ�����
		{
			if(strstr((const char *)esp8266_buf, res) != NULL)		//����������ؼ���
			{
				ESP8266_Clear();									//��ջ���
				
				return 0;
			}
		}
		
		DelayXms(10);
	}
	
	return 1;

}

//==========================================================
//	�������ƣ�	ESP8266_SendData
//
//	�������ܣ�	��������
//
//	��ڲ�����	data������
//				len������
//
//	���ز�����	��
//
//	˵����		
//==========================================================
void ESP8266_SendData(unsigned char *data, unsigned short len)
{

	char cmdBuf[32];
	
	ESP8266_Clear();								//��ս��ջ���
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);		//��������
	if(!ESP8266_SendCmd(cmdBuf, ">"))				//�յ���>��ʱ���Է�������
	{
		Usart_SendString(USART1, data, len);		//�����豸������������
	}

}

//==========================================================
//	�������ƣ�	ESP8266_GetIPD
//
//	�������ܣ�	��ȡƽ̨���ص�����
//
//	��ڲ�����	�ȴ���ʱ��(����10ms)
//
//	���ز�����	ƽ̨���ص�ԭʼ����
//
//	˵����		��ͬ�����豸���صĸ�ʽ��ͬ����Ҫȥ����
//				��ESP8266�ķ��ظ�ʽΪ	"+IPD,x:yyy"	x�������ݳ��ȣ�yyy����������
//==========================================================
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{

	char *ptrIPD = NULL;
	
	do
	{
		if(ESP8266_WaitRecive() == REV_OK)								//����������
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");				//������IPD��ͷ
			if(ptrIPD == NULL)											//���û�ҵ���������IPDͷ���ӳ٣�������Ҫ�ȴ�һ�ᣬ�����ᳬ���趨��ʱ��
			{
				//UsartPrintf(USART_DEBUG, "\"IPD\" not found\r\n");
			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');							//�ҵ�':'
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;
				
			}
		}
		
		DelayXms(5);													//��ʱ�ȴ�
	} while(timeOut--);
	
	return NULL;														//��ʱ��δ�ҵ������ؿ�ָ��

}

//==========================================================
//	�������ƣ�	ESP8266_Init
//
//	�������ܣ�	��ʼ��ESP8266
//
//	��ڲ�����	��
//
//	���ز�����	��
//
//	˵����		
//==========================================================
void ESP8266_Init(void)
{
	Delay_Init();
	ESP8266_Clear();
//AT+RESTORE                                       #�ָ���������
//AT+CWMODE=1                                      #����ESP8266����ģʽΪSTA
//AT+RST                                           #��λ
//AT+CWJAP="·�����˺�","����"                     #����·����
//AT+CIPMODE=1                                     #����͸��ģʽ
//AT+CIPSTART="TCP","192.168.1.11",8266           #����TCP����������λ����
//AT+CIPSEND                                       #����͸��
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
//	�������ƣ�	USART_IRQHandler
//
//	�������ܣ�	����2�շ��ж�
//
//	��ڲ�����	��
//
//	���ز�����	��
//
//	˵����		
//==========================================================
void USART1_IRQHandler(void)
{

	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) //�����ж�
	{
		if(esp8266_cnt >= sizeof(esp8266_buf))	esp8266_cnt = 0; //��ֹ���ڱ�ˢ��
		esp8266_buf[esp8266_cnt++] = USART1->DR;
		
		USART_ClearFlag(USART1, USART_FLAG_RXNE);
	}

}


// ����ȫ�ֱ����������麯��
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
