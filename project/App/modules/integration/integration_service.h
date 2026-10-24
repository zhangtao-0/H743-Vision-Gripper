#ifndef INTEGRATION_SERVICE_H
#define INTEGRATION_SERVICE_H
#include "platform_hal.h"
typedef struct {
    uint8_t clocks_ok, lcd_bus_ready, rtc_ready, camera_ready;
    uint32_t loop_max_ms, last_init_ms;
} IntegrationStatus;
void Integration_Init(void);
void Integration_Task(uint32_t now);
void Integration_StatusReport(void);
void Integration_LcdTest(void);
void Integration_SetStream(uint8_t enable);
const IntegrationStatus *Integration_GetStatus(void);
#endif
