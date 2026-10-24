#include "vendor_log.h"
#include "vofa_protocol.h"
#include "usb_console.h"
#include <stdarg.h>
static uint8_t enabled=1;
void VendorLog_Enable(uint8_t on){enabled=(on!=0);}
int VendorLog(const char *format, ...)
{
    if(!enabled)return 0;
    char line[192]; va_list ap; int n;
    va_start(ap, format); n = vsnprintf(line, sizeof(line), format, ap); va_end(ap);
    if (n > 0) {
        uint16_t len = (uint16_t)((n < (int)sizeof(line)) ? n : (int)sizeof(line) - 1);
        (void)VOFA_Protocol_SendRaw((const uint8_t *)line, len);
        UsbConsole_Write((const uint8_t *)line, len);
    }
    return n;
}
