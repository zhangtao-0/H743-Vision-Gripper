#ifndef VENDOR_LOG_H
#define VENDOR_LOG_H
#include <stdint.h>
#include <stdio.h>
void VendorLog_Enable(uint8_t enable);
int VendorLog(const char *format, ...);
#endif
