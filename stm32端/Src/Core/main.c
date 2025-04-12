#include "main.h"
#include "ov2640.h"
#include "esp8266.h"
#include "OLED.h"
#include "systick.h"
#include "uart.h"
#include "MFRC522.h"
#include "button.h"
#include "sg90.h"
#include "beep.h"

#include <string.h>  // ???????????
#include <stddef.h>  // ???????????
#include <stdio.h>
#include "stm32f10x_it.h"


#define JPEG_BUFFER_LENGTH 16 * 1024   //16
#define OUTPUT_IMAGE_WIDTH 160 //160
#define OUTPUT_IMAGE_HEIGHT 120 //120

#define OV2640_CLOCK 4    //4
#define OV2640_DVP_CLOCK 48   //48

uint8_t jpeg_buffer[JPEG_BUFFER_LENGTH] = {0};
extern volatile uint8_t esp8266_cmd_flag;


void Send_CardID(uint8_t *card_serial) {
    char cmd[64];
    sprintf(cmd, "CARD:%02X%02X%02X%02X\r\n", 
            card_serial[0], card_serial[1], 
            card_serial[2], card_serial[3]);
    
    OLED_Clear();
    OLED_ShowString(0, 0, "Sending Card:", OLED_8X16);
    OLED_Update();
    
    ESP8266_SendData((uint8_t*)cmd, strlen(cmd));
    Delay_ms(100); // ??????????
}


int main(void)
{
    // Clock
    RCC_ClocksTypeDef Clock;
    RCC_GetClocksFreq(&Clock);
    SysTick_Init();  // 初始化系统滴答定时器

    OLED_Init();
    UART1_Init(115200);

    OV2640_Init();
    
    OLED_ShowString(0, 48, "Yoitsu Poet", OLED_8X16);
    OLED_Update();
    
    ESP8266_Init();
    Delay_ms(500);  // 等待 ESP8266 初始化完成

    OLED_Init();        
    OLED_Clear();    
    OLED_ShowString(0, 48, "connected", OLED_8X16);
    OLED_Update();
    MFRC522_Init();
    Button_Init();
    BEEP_Init();
    SG90_Init();
    SG90_SetAngle(0);  // 确保初始化时舵机在0°位置（关门状态）
    LED_Init();
    BEEP_ShortBeep();
		
	
    // OV2640 Setting
    OV2640_SetClockDivision(OV2640_CLOCK, OV2640_DVP_CLOCK);
    OV2640_SetOutputJPEG();
    OV2640_SetOutputSize(OUTPUT_IMAGE_WIDTH, OUTPUT_IMAGE_HEIGHT);
    OV2640_TIM_DMA_Init(jpeg_buffer, JPEG_BUFFER_LENGTH);
    OV2640_TIM_DMA_Start();		
		
    uint8_t status;
    uint8_t card_serial[4];
		
		
		
		typedef enum {
				STATE_IDLE,
				STATE_RFID_PROCESSING,
				STATE_BUTTON_PROCESSING,
				STATE_DOOR_OPEN
		} SystemState;

		SystemState current_state = STATE_IDLE;

    while (1) {
        // ??????????????
        ESP8266_CheckCommand();
			
        // ??????????????
					if (button_pressed) {
							button_pressed = 0;
							BEEP_ShortBeep();
							
							OLED_Clear();
							OLED_ShowString(0, 0, "Button Pressed", OLED_8X16);
							OLED_ShowString(0, 32, "Sending...", OLED_8X16);
							OLED_Update();
							
							// ????????????
							ESP8266_SendData((uint8_t*)"BUTTON_EVENT\r\n", 14);
							Delay_ms(50);  // ???????
							
							// ?????????????????飩
							OV2640_TransmitFrame_JPEG_UART1_TIM_DMA(jpeg_buffer, JPEG_BUFFER_LENGTH);
							
							OLED_ShowString(0, 48, "Photo Sent", OLED_8X16);
							GPIO_ResetBits(GPIOC, GPIO_Pin_13); // LED????
							BEEP_Success();
							OLED_Update();
							Delay_ms(300); // ???????
					}


				
				// ???????????
				unsigned char *cmd = ESP8266_GetIPD(100);
				if(cmd != NULL) {
						if(strstr((char*)cmd, "OPEN_DOOR")) {
								// 首先确认门是关闭状态
								SG90_SetAngle(0);
								Delay_ms(100);  // 短暂延时确保初始位置
								
								// 打开门（旋转到90°）
								SG90_SetAngle(90);
								
								// 显示门已打开
								OLED_Clear();
								OLED_ShowString(0, 0, "Door Opened", OLED_8X16);
								BEEP_Success();
								OLED_Update();
								
								// 等待5秒
								Delay_s(5);
								
								// 关闭门（恢复到0°）
								SG90_SetAngle(0);
								
								// 显示门已关闭
								OLED_Clear();
								OLED_ShowString(0, 0, "Door Closed", OLED_8X16);
								BEEP_Success();
								OLED_Update();
						}
						ESP8266_Clear();
				}

        // RFID???
        uint8_t card_type[2];
        
        // ???λRFID??????????????????
        MFRC522_Reset();
        MFRC522_AntennaOn();
				
				
				// RFID卡片检测部分修改
				status = MFRC522_Request(PICC_REQIDL, card_type);  // 检测卡片
				if(status == MI_OK) {
						Delay_ms(50); // 延时50ms，避免连续读取
						
						// 如果检测到，读取卡片序列号
						status = MFRC522_Anticoll(card_serial);
						if(status == MI_OK) {
								// 显示已检测到卡片
								OLED_Clear();
								OLED_ShowString(0, 0, "Card Detected", OLED_8X16);
								OLED_Update();
								
								// 发送卡片ID到上位机
								Send_CardID(card_serial);
								
								// 发送图像
								OV2640_TransmitFrame_JPEG_UART1_TIM_DMA(jpeg_buffer, JPEG_BUFFER_LENGTH);
								
								// 显示等待验证信息
								OLED_Clear();
								OLED_ShowString(0, 0, "Card Sent", OLED_8X16);
								OLED_ShowString(0, 16, "Waiting Auth...", OLED_8X16);
								OLED_Update();
								
								// 等待上位机响应（最多等待3秒）
								uint8_t auth_timeout = 30; // 3秒 (30 * 100ms)
								while(auth_timeout--) {
										unsigned char *response = ESP8266_GetIPD(100);
										if(response != NULL) {
												if(strstr((char*)response, "OPEN_DOOR")) {
														// 首先确认门是关闭状态
														SG90_SetAngle(0);
														Delay_ms(100);  // 短暂延时确保初始位置
														
														// 打开门（旋转到90°）
														SG90_SetAngle(90);
														
														// 显示门已打开
														OLED_Clear();
														OLED_ShowString(0, 0, "Card Valid", OLED_8X16);
														OLED_ShowString(0, 16, "Door Opened", OLED_8X16);
														BEEP_Success();
														OLED_Update();
														
														// 等待5秒
														Delay_s(5);
														
														// 关闭门（恢复到0°）
														SG90_SetAngle(0);
														
														// 显示门已关闭
														OLED_Clear();
														OLED_ShowString(0, 0, "Door Closed", OLED_8X16);
														BEEP_Success();
														OLED_Update();
														
														ESP8266_Clear();
														break; // 跳出等待循环
												}
												ESP8266_Clear();
										}
										Delay_ms(100); // 每100ms检查一次
								}
								
								// 如果超时未收到响应
								if(auth_timeout == 0) {
										OLED_Clear();
										OLED_ShowString(0, 0, "Auth Timeout", OLED_8X16);
										OLED_ShowString(0, 16, "Access Denied", OLED_8X16);
										BEEP_ShortBeep();
										BEEP_ShortBeep();
										OLED_Update();
										Delay_ms(1000);
								}
								
								// 暂时休眠卡片
								MFRC522_Halt();
								MFRC522_AntennaOff();  // 关闭天线
								
								// 延时一段时间，防止连续重复读取
								Delay_ms(1000);
								
								// 重新开启天线，准备下一次读取
								MFRC522_AntennaOn();
						}
				}
		

        // ??????????????
        Delay_ms(50);
							
    }
	}

