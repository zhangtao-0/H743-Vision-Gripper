#ifndef USB_CONSOLE_H
#define USB_CONSOLE_H
#include <stdint.h>
void UsbConsole_Init(void);
void UsbConsole_Task(void);
void UsbConsole_OnReceive(const uint8_t *data, uint32_t size);
void UsbConsole_Write(const uint8_t *data, uint16_t size);
uint8_t UsbConsole_Connected(void);
uint32_t UsbConsole_Dropped(void);
/* Buffers stay leased by caller until Busy becomes false; one main-loop producer. */
uint8_t UsbConsole_StreamStart(const uint8_t *header,uint16_t header_size,const uint8_t *payload,uint32_t payload_size);
uint8_t UsbConsole_StreamBusy(void);
void UsbConsole_StreamAbort(void);
#endif
