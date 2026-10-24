#include "usb_console.h"
#include "platform_hal.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "integration_console.h"
#include <string.h>
extern USBD_HandleTypeDef hUsbDeviceFS;
#define USB_RING 1024U
/* RX echo producer is ISR only; message producer is main-loop only.
 * Separate queues avoid ISR/main write-head races. */
static uint8_t s_echo[USB_RING], s_messages[USB_RING], s_tx[64];
static volatile uint16_t s_ehead, s_etail;
static uint16_t s_mhead, s_mtail;
static volatile uint32_t s_dropped;
static const uint8_t *s_header,*s_payload;
static uint32_t s_stream_pos,s_payload_size;
static uint16_t s_header_size;
static uint8_t s_stream;
uint8_t UsbConsole_StreamBusy(void){return s_stream;}
void UsbConsole_StreamAbort(void){s_stream=0;}
uint8_t UsbConsole_StreamStart(const uint8_t *header,uint16_t hn,const uint8_t *payload,uint32_t pn){
 uint16_t echo_head=s_ehead;
 uint16_t echo_tail=s_etail;
 if(!header||!payload||!hn||!pn||s_stream||!UsbConsole_Connected()||s_mhead!=s_mtail||echo_head!=echo_tail)return 0;
 s_header=header;s_payload=payload;s_header_size=hn;s_payload_size=pn;s_stream_pos=0;s_stream=1;return 1;
}

uint8_t UsbConsole_Connected(void)
{ return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != 0; }
uint32_t UsbConsole_Dropped(void) { return s_dropped; }
void UsbConsole_Init(void) { MX_USB_DEVICE_Init(); }

void UsbConsole_OnReceive(const uint8_t *data, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        uint16_t next;
        if (IntegrationConsole_PushByte(data[i], 1U, 1U)) continue;
        next = (uint16_t)((s_ehead + 1U) % USB_RING);
        if (next == s_etail) { s_dropped++; continue; }
        s_echo[s_ehead] = data[i]; __DMB(); s_ehead = next;
    }
}

void UsbConsole_Write(const uint8_t *data, uint16_t size)
{
    uint16_t i;
    if (!data || !UsbConsole_Connected()) return;
    for (i = 0; i < size; i++) {
        uint16_t next = (uint16_t)((s_mhead + 1U) % USB_RING);
        if (next == s_mtail) { s_dropped++; break; }
        s_messages[s_mhead] = data[i]; s_mhead = next;
    }
}

void UsbConsole_Task(void)
{
    USBD_CDC_HandleTypeDef *cdc;
    uint16_t cursor, n = 0; uint8_t echo;
    if (!UsbConsole_Connected()) { s_etail = s_ehead; s_mtail = s_mhead; s_stream=0; return; }
    cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (cdc->TxState) return; /* s_tx remains owned by the endpoint until completion */
    if(s_stream){
        uint32_t pos=s_stream_pos,total=s_header_size+s_payload_size;
        while(n<sizeof(s_tx)&&pos<total){s_tx[n++]=pos<s_header_size?s_header[pos]:s_payload[pos-s_header_size];pos++;}
        if(n && CDC_Transmit_FS(s_tx,n)==USBD_OK){s_stream_pos=pos;if(pos==total)s_stream=0;}
        return; /* text waits between complete frames; UART1 is independent */
    }
    echo = (s_mhead == s_mtail);
    cursor = echo ? s_etail : s_mtail;
    while (n < sizeof(s_tx) && cursor != (echo ? s_ehead : s_mhead)) {
        s_tx[n++] = echo ? s_echo[cursor] : s_messages[cursor];
        cursor = (uint16_t)((cursor + 1U) % USB_RING);
    }
    if (n && CDC_Transmit_FS(s_tx, n) == USBD_OK) {
        if (echo) s_etail = cursor; else s_mtail = cursor;
    }
}
