#ifndef _SCCB_H
#define _SCCB_H

#include "main.h"

void SWSCCB_Init(void);
void SWSCCB_WriteReg(uint8_t id_address, uint8_t sub_address, uint8_t data);
uint8_t SWSCCB_ReadReg(uint8_t id_address, uint8_t sub_address);

// void SCCB_Init(void);
// void SCCB_WriteReg(uint8_t id_address, uint8_t sub_address, uint8_t data);
// uint8_t SCCB_ReadReg(uint8_t id_address, uint8_t sub_address);
#endif // _SCCB_H
