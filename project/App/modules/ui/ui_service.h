#ifndef UI_SERVICE_H
#define UI_SERVICE_H
#include <stdint.h>
uint8_t UiService_Rotation(void);
void UiService_SetRotation(uint8_t rotation);
void UiService_Init(void);
void UiService_Input(uint32_t now,uint8_t up,uint8_t down,uint8_t ok,uint8_t back);
void UiService_Task(uint32_t now);
void UiService_LcdTest(uint8_t active);
#endif
