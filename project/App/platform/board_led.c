#include "board_gpio.h"
#include "board_led.h"
#include "board_config.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_low;
    BoardLedPattern pattern;
    uint32_t start;
} Led;

static Led s_led[BOARD_LED_COUNT] = {
    {BOARD_LED_HEARTBEAT_PORT, BOARD_LED_HEARTBEAT_PIN, BOARD_LED_HEARTBEAT_ACTIVE_LOW, BOARD_LED_OFF, 0U},
    {BOARD_LED_STATUS_PORT, BOARD_LED_STATUS_PIN, BOARD_LED_STATUS_ACTIVE_LOW, BOARD_LED_OFF, 0U},
    {BOARD_LED_FAULT_PORT, BOARD_LED_FAULT_PIN, BOARD_LED_FAULT_ACTIVE_LOW, BOARD_LED_OFF, 0U}
};

static void Led_Write(Led *led, uint8_t on)
{
    HAL_GPIO_WritePin(led->port, led->pin,
        ((on != 0U) != (led->active_low != 0U)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BoardLed_Init(void)
{
    uint32_t i;
    GPIO_InitTypeDef gpio = {0};
    /* Each LED may live on a different GPIO port. */
    for (i = 0U; i < BOARD_LED_COUNT; i++) {
        BoardGpio_EnableClock(s_led[i].port);
        s_led[i].pattern = BOARD_LED_OFF;
        s_led[i].start = HAL_GetTick();
        Led_Write(&s_led[i], 0U); /* 先设置输出锁存，避免切输出时短暂点亮。 */
        gpio.Pin = s_led[i].pin;
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(s_led[i].port, &gpio);
    }
}

void BoardLed_Set(BoardLedId id, BoardLedPattern pattern)
{
    if ((unsigned)id >= BOARD_LED_COUNT || (unsigned)pattern > BOARD_LED_BLINK_5HZ) return;
    if (s_led[id].pattern != pattern) {
        s_led[id].pattern = pattern;
        s_led[id].start = HAL_GetTick();
    }
}

void BoardLed_Task(uint32_t now_ms)
{
    uint32_t i;
    for (i = 0U; i < BOARD_LED_COUNT; i++) {
        uint32_t elapsed = now_ms - s_led[i].start;
        uint8_t on = 0U;
        switch (s_led[i].pattern) {
            case BOARD_LED_ON: on = 1U; break;
            case BOARD_LED_HEARTBEAT_1HZ: on = ((elapsed % 1000U) < 100U); break;
            case BOARD_LED_BLINK_2HZ: on = ((elapsed % 500U) < 250U); break;
            case BOARD_LED_BLINK_5HZ: on = ((elapsed % 200U) < 100U); break;
            default: break;
        }
        Led_Write(&s_led[i], on);
    }
}
