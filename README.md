# RFID-Cam-DoorGuard

## 项目介绍
本项目是一个基于STM32F103CBT6微控制器的智能门禁系统
> 精简代码后可以达到200×200像素的图片传输，C8T6同样可以使用，但是只能稳定传输160×120的图片

| 模块           | 功能描述                     | 
|----------------|------------------------------|
| **STM32F103C8** | 主控芯片，协调各模块通信 |
| **MFRC522**     | RFID卡号读取与身份验证 |
| **OV2640**      | 实时JPEG图像采集与传输 |
| **ESP8266**     | Wi-Fi联网，远程指令收发 |
| **SG90舵机**    | 门锁驱动 |
| **蜂鸣器**    | 操作成功/失败提示 |
| **0.96寸4pin屏幕**    | 状态直观显示 |

## 使用方法
1.修改`esp8266.c`中
```
#define ESP8266_WIFI_INFO		"AT+CWJAP=\"Test\",\"test12345\"\r\n"
#define WINDOWS_IP    "AT+CIPSTART=\"TCP\",\"192.168.203.60\",8089\r\n"
```
Test为WiFi名，test12345为WiFi密码，192.168.203.60为电脑IP
> 手机创建局域网后，可在手机终端输入`sudo ip neigh`查看esp8266IP和电脑ip

2.烧录代码，启动电脑上位机程序
上位机有自动填写IP的功能，请仔细检查。服务端IP为电脑IP，客户端IP为esp8266IP  **请仔细检查！**
连接成功后，上位机会显示已连接的IP和自动分配的端口，下位机也会显示connected

3.开门主要有两种方式，门禁卡开门和按钮拍照开门
  - 门禁卡开门：
    - 第一次启动上位机会自动生成一个`card.txt`，下位机刷卡后，可从上位机记录卡片id并记录到`card.txt`中，使其变成有效卡
    - 刷卡会同时进行拍照，以明确卡片使用者（后续可连接树莓派，在确保有人时才识别卡片，防止有人遮挡摄像头）
    - 
  - 按钮开门
  - 按下按钮后，摄像头会将拍到的照片传输到上位机，上位机有30秒的时间确认来访者并决定是否开门

4.无论以何种方式触发系统，上位机都会在`access_log.txt`留有记录，并保留图片
> xxxx年xx月xx日xx:xx:xx--图片/卡片id--开门/未开门

**按钮无法被上拉至3.3v，经测量只有0.95v
不确定是pcb板问题还是布线问题
目前已将按钮旋转180°以便解决布线问题
此外可以考虑将摄像头等模块布线改为等长线**
