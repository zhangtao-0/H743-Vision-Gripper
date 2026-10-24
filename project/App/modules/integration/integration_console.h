#ifndef INTEGRATION_CONSOLE_H
#define INTEGRATION_CONSOLE_H
#include <stdint.h>
/* ISR-safe single producer per channel; text starts with @ only at protocol idle. */
uint8_t IntegrationConsole_PushByte(uint8_t byte, uint8_t channel, uint8_t allow_start);
uint8_t IntegrationConsole_RequestHelp(uint8_t group);
const char *IntegrationConsole_HelpName(uint8_t group);
void IntegrationConsole_Quiet(void);
void IntegrationConsole_Task(void);
uint32_t IntegrationConsole_Dropped(void);
#endif
