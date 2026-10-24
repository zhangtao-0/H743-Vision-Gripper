#ifndef BOARD_LED_H
#define BOARD_LED_H
#include <stdint.h>

typedef enum { BOARD_LED_HEARTBEAT, BOARD_LED_STATUS, BOARD_LED_FAULT, BOARD_LED_COUNT } BoardLedId;
typedef enum { BOARD_LED_OFF, BOARD_LED_ON, BOARD_LED_HEARTBEAT_1HZ,
               BOARD_LED_BLINK_2HZ, BOARD_LED_BLINK_5HZ } BoardLedPattern;

void BoardLed_Init(void);
void BoardLed_Set(BoardLedId id, BoardLedPattern pattern);
void BoardLed_Task(uint32_t now_ms);
#endif
