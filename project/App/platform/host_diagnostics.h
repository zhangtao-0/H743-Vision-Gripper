#ifndef HOST_DIAGNOSTICS_H
#define HOST_DIAGNOSTICS_H
#include "platform_hal.h"

typedef struct {
    volatile uint32_t rx_bytes, tx_bytes, rx_overflow, uart_errors, tx_errors, rearm_errors;
} HostDiagnosticsStatus;
void HostDiagnostics_Init(void);
void HostDiagnostics_Task(void);
void HostDiagnostics_RxComplete(void);
void HostDiagnostics_Error(void);
const HostDiagnosticsStatus *HostDiagnostics_GetStatus(void);
#endif
