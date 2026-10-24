#ifndef INPUT_CONTROL_H
#define INPUT_CONTROL_H
#include <stdint.h>
/* Logical input IDs are stable even if GPIO is moved: 1..4 menu, 5/6 claw A/B, 7 motor/EC11 push. */
typedef enum {INPUT_MIXED=0,INPUT_PHYSICAL=1,INPUT_REMOTE=2} InputMode;
#define INPUT_KEY_COUNT 7U
#define INPUT_KEY_MASK 127U
#define INPUT_MOTOR_KEY 7U
#define INPUT_SOURCE_COUNT 2U /* UART text, USB text; legacy F5 remains in its existing merger. */
#define INPUT_MAX_HOLD_MS 5000U
/* Independent runtime grants: 0 STS, 1 motor. Boot OFF; never enables outputs. */
uint8_t Input_DebugEnabled(uint8_t target);
uint8_t Input_DebugSet(uint8_t target,uint8_t on);
void Input_LegacyHeld(uint8_t held);
InputMode Input_GetMode(void);
uint8_t Input_SetMode(InputMode mode);
uint8_t Input_Resolve(uint8_t key,uint8_t physical,uint32_t now);
uint8_t Input_Pulse(uint8_t source,uint8_t mask,uint32_t ms,uint32_t now);
uint8_t Input_Level(uint8_t source,uint8_t key,uint8_t down,uint32_t now);
void Input_Release(uint8_t source);
uint8_t Input_SourceMask(uint8_t source,uint32_t now);
#endif
