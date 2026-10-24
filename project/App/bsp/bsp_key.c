#include "board_gpio.h"
/*********************************************************************************************************************
 * @file    bsp_key.c
 * @brief   通用按键扫描与事件识别实现。
 *********************************************************************************************************************/

#include "bsp_key.h"

/* Main-loop writes are atomic against the motor's 1ms input sampling ISR. */
static volatile InputMode input_mode=INPUT_MIXED;
static volatile uint8_t input_physical[INPUT_KEY_COUNT],legacy_held;
void Input_LegacyHeld(uint8_t held){legacy_held=held;}
static volatile uint8_t debug_grants;
uint8_t Input_DebugEnabled(uint8_t target){return target<2U?(uint8_t)((debug_grants>>target)&1U):0;}
uint8_t Input_DebugSet(uint8_t target,uint8_t on){
 uint32_t lock,now=HAL_GetTick();uint8_t i;
 if(target>=2U)return 0;
 lock=__get_PRIMASK();__disable_irq();
 if(legacy_held){__set_PRIMASK(lock);return 0;}
 /* Do not turn an in-flight press/release into an action for a new recipient. */
 for(i=4;i<INPUT_KEY_COUNT;i++)if(input_physical[i]){__set_PRIMASK(lock);return 0;}
 for(i=5;i<=Key_Count;i++)if(Key_GetFlag((KeyCode_t)i)){__set_PRIMASK(lock);return 0;}
 for(i=0;i<INPUT_SOURCE_COUNT;i++)if(Input_SourceMask(i,now)&112U){__set_PRIMASK(lock);return 0;}
 if(on)debug_grants|=(uint8_t)(1U<<target);else debug_grants&=(uint8_t)~(1U<<target);
 Key_ClearAll();__set_PRIMASK(lock);return 1;
}
static volatile uint8_t input_mask[INPUT_SOURCE_COUNT];
static volatile uint32_t input_until[INPUT_SOURCE_COUNT][INPUT_KEY_COUNT];
InputMode Input_GetMode(void){return input_mode;}
uint8_t Input_SourceMask(uint8_t source,uint32_t now){
 uint8_t i,mask=0;if(source>=INPUT_SOURCE_COUNT)return 0;
 for(i=0;i<INPUT_KEY_COUNT;i++)if((input_mask[source]&(1U<<i))&&(int32_t)(now-input_until[source][i])<0)mask|=(uint8_t)(1U<<i);
 return mask;
}
uint8_t Input_Resolve(uint8_t key,uint8_t physical,uint32_t now){
 uint8_t source,remote=0;if(!key||key>INPUT_KEY_COUNT)return 0;
 input_physical[key-1U]=(physical!=0);
 if(input_mode!=INPUT_PHYSICAL)for(source=0;source<INPUT_SOURCE_COUNT;source++)remote|=Input_SourceMask(source,now)&(1U<<(key-1U));
 return (uint8_t)((input_mode!=INPUT_REMOTE&&physical)||remote);
}
uint8_t Input_Pulse(uint8_t source,uint8_t mask,uint32_t ms,uint32_t now){
 uint32_t lock;uint8_t i;
 if(source>=INPUT_SOURCE_COUNT||!mask||(mask&~INPUT_KEY_MASK)||ms<40U||ms>INPUT_MAX_HOLD_MS||input_mode==INPUT_PHYSICAL)return 0;
 lock=__get_PRIMASK();__disable_irq();
 if(Input_SourceMask(source,now)&mask){__set_PRIMASK(lock);return 0;}
 for(i=0;i<INPUT_KEY_COUNT;i++)if(mask&(1U<<i))input_until[source][i]=now+ms;
 input_mask[source]|=mask;__set_PRIMASK(lock);return 1;
}
uint8_t Input_Level(uint8_t source,uint8_t key,uint8_t down,uint32_t now){
 uint32_t lock;uint8_t bit;
 if(source>=INPUT_SOURCE_COUNT||!key||key>INPUT_KEY_COUNT||(down&&input_mode==INPUT_PHYSICAL))return 0;
 bit=(uint8_t)(1U<<(key-1U));lock=__get_PRIMASK();__disable_irq();
 if(down){input_until[source][key-1U]=now+INPUT_MAX_HOLD_MS;input_mask[source]|=bit;}
 else input_mask[source]&=(uint8_t)~bit;
 __set_PRIMASK(lock);return 1;
}
void Input_Release(uint8_t source){uint32_t lock;if(source>=INPUT_SOURCE_COUNT)return;lock=__get_PRIMASK();__disable_irq();input_mask[source]=0;__set_PRIMASK(lock);}
uint8_t Input_SetMode(InputMode mode){
 uint32_t lock,now=HAL_GetTick();uint8_t i;
 if((unsigned)mode>INPUT_REMOTE||legacy_held)return 0;
 for(i=0;i<INPUT_KEY_COUNT;i++)if(input_physical[i])return 0;
 /* Mode changes cannot synthesize a release for an ongoing physical/virtual hold. */
 for(i=1;i<=Key_Count;i++)if(Key_GetFlag((KeyCode_t)i))return 0;
 for(i=0;i<INPUT_SOURCE_COUNT;i++)if(Input_SourceMask(i,now))return 0;
 lock=__get_PRIMASK();__disable_irq();input_mode=mode;
 for(i=0;i<INPUT_SOURCE_COUNT;i++)input_mask[i]=0;
 __set_PRIMASK(lock);return 1;
}
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t mode;
    uint32_t pull;
    uint32_t speed;
    GPIO_PinState is_pressed;
    uint8_t enable;
    KeyTriggerMode_t trigger_mode;
} KeyConfig_t;

typedef struct {
    uint8_t raw_pressed;
    uint8_t stable_pressed;
    uint8_t long_reported;
    uint8_t click_count;
    uint32_t last_change_tick;
    uint32_t press_start_tick;
    uint32_t click_tick;
    uint16_t events;
} KeyRuntime_t;

typedef struct {
    KeyCode_t code;
    KeyConfig_t config;
    KeyRuntime_t runtime;
    uint8_t flag;
} KeyManager_t;

static KeyManager_t s_key_table[Key_TableSize] = {
    {Key_None, {0, 0U, 0U, 0U, 0U, GPIO_PIN_RESET, 0U, KEY_TRIGGER_NONE},
     {0U, 0U, 0U, 0U, 0UL, 0UL, 0UL, 0U}, 0U},
    {Key_Task1, {KEY_Task1_PORT, KEY_Task1_PIN, KEY_Task1_MODE, KEY_Task1_PULL, KEY_Task1_SPEED,
     KEY_Task1_ACTIVE_STATE, 1U, KEY_TRIGGER_CLICK},
     {0U, 0U, 0U, 0U, 0UL, 0UL, 0UL, 0U}, 0U},
    {Key_Task2, {KEY_Task2_PORT, KEY_Task2_PIN, KEY_Task2_MODE, KEY_Task2_PULL, KEY_Task2_SPEED,
     KEY_Task2_ACTIVE_STATE, 1U, KEY_TRIGGER_LONG_PRESS},
     {0U, 0U, 0U, 0U, 0UL, 0UL, 0UL, 0U}, 0U},
    {Key_Task3, {KEY_Task3_PORT, KEY_Task3_PIN, KEY_Task3_MODE, KEY_Task3_PULL, KEY_Task3_SPEED,
     KEY_Task3_ACTIVE_STATE, 1U, KEY_TRIGGER_NONE},
     {0U, 0U, 0U, 0U, 0UL, 0UL, 0UL, 0U}, 0U},
    {Key_Task4, {KEY_Task4_PORT, KEY_Task4_PIN, KEY_Task4_MODE, KEY_Task4_PULL, KEY_Task4_SPEED,
     KEY_Task4_ACTIVE_STATE, 1U, KEY_TRIGGER_NONE},
     {0U, 0U, 0U, 0U, 0UL, 0UL, 0UL, 0U}, 0U},
    {Key_Task5, {KEY_Task5_PORT, KEY_Task5_PIN, KEY_Task5_MODE, KEY_Task5_PULL, KEY_Task5_SPEED, KEY_Task5_ACTIVE_STATE, 1U, KEY_TRIGGER_NONE}, {0U,0U,0U,0U,0UL,0UL,0UL,0U},0U},
    {Key_Task6, {KEY_Task6_PORT, KEY_Task6_PIN, KEY_Task6_MODE, KEY_Task6_PULL, KEY_Task6_SPEED, KEY_Task6_ACTIVE_STATE, 1U, KEY_TRIGGER_NONE}, {0U,0U,0U,0U,0UL,0UL,0UL,0U},0U}
};

static KeyManager_t *Key_GetManager(KeyCode_t code);
static void Key_ProcessOne(KeyManager_t *key, uint32_t now);
static uint8_t Key_ReadRawPressed(KeyManager_t *key);

// 初始化按键 GPIO 和事件状态。
void Key_Init(void)
{
    GPIO_InitTypeDef gpio;
    uint32_t now;
    uint8_t i;
    KeyManager_t *key;

    debug_grants=0;input_mode=INPUT_MIXED;Input_Release(0);Input_Release(1);legacy_held=0;
    for(i=0;i<INPUT_KEY_COUNT;i++)input_physical[i]=0;
    /* Clock follows each configured physical port. */
    now = HAL_GetTick();

    for (i = 1U; i < (uint8_t)Key_TableSize; i++) {
        key = &s_key_table[i];
        if (key->config.enable == 0U) {
            continue;
        }
        BoardGpio_EnableClock(key->config.port);
        gpio.Pin = key->config.pin;
        gpio.Mode = key->config.mode;
        gpio.Pull = key->config.pull;
        gpio.Speed = key->config.speed;
        HAL_GPIO_Init(key->config.port, &gpio);

        key->runtime.raw_pressed = Key_ReadRawPressed(key);
        key->runtime.stable_pressed = key->runtime.raw_pressed;
        key->runtime.long_reported = 0U;
        key->runtime.click_count = 0U;
        key->runtime.last_change_tick = now;
        key->runtime.press_start_tick = now;
        key->runtime.click_tick = now;
        key->runtime.events = KEY_EVENT_NONE;
        key->flag = 0U;
    }
}

// 扫描全部按键：GPIO -> 消抖 -> 通用按键事件。
void Key_Task(void)
{
    uint8_t i;
    uint32_t now;

    now = HAL_GetTick();
    for (i = 1U; i < (uint8_t)Key_TableSize; i++) {
        Key_ProcessOne(&s_key_table[i], now);
    }
}

// 读取并清除一个按键的全部待处理事件。
uint16_t Key_TakeEvent(KeyCode_t code)
{
    uint16_t events;
    KeyManager_t *key;

    key = Key_GetManager(code);
    if (key == 0) {
        return KEY_EVENT_NONE;
    }
    events = key->runtime.events;
    key->runtime.events = KEY_EVENT_NONE;
    return events;
}

// 读取兼容 flag。
uint8_t Key_Scan(KeyCode_t code)
{
    KeyManager_t *key;
    key = Key_GetManager(code);
    return (key != 0) ? key->flag : 0U;
}

// 读取并清除兼容 flag。
uint8_t Key_ReadAndClear(KeyCode_t code)
{
    uint8_t value;
    value = Key_Scan(code);
    if (value != 0U) {
        Key_Clear(code);
    }
    return value;
}

// 读取消抖后的有效按下状态：物理/模拟按模式合并。
uint8_t Key_GetFlag(KeyCode_t code)
{
    KeyManager_t *key;
    key = Key_GetManager(code);
    return (key != 0) ? key->runtime.stable_pressed : 0U;
}

// 清除兼容 flag。
void Key_Clear(KeyCode_t code)
{
    KeyManager_t *key;
    key = Key_GetManager(code);
    if (key != 0) {
        key->flag = 0U;
    }
}

// 清除全部兼容 flag 和待处理事件。
void Key_ClearAll(void)
{
    uint8_t i;
    for (i = 1U; i < (uint8_t)Key_TableSize; i++) {
        s_key_table[i].flag = 0U;
        s_key_table[i].runtime.events = KEY_EVENT_NONE;
    }
}

// 翻转兼容 flag。
void Key_Toggle(KeyCode_t code)
{
    KeyManager_t *key;
    key = Key_GetManager(code);
    if (key != 0) {
        key->flag = (key->flag == 0U) ? 1U : 0U;
    }
}

// 设置兼容 flag。
void Key_SetFlag(KeyCode_t code, uint8_t value)
{
    KeyManager_t *key;
    key = Key_GetManager(code);
    if (key != 0) {
        key->flag = (value == 0U) ? 0U : 1U;
    }
}

// 初始化独立按键手势状态。
void KeyGesture_Init(KeyGesture_t *gesture)
{
    if (gesture == 0) {
        return;
    }
    gesture->pressed = 0U;
    gesture->long_reported = 0U;
    gesture->click_count = 0U;
    gesture->press_start_tick = 0UL;
    gesture->click_tick = 0UL;
    gesture->events = KEY_EVENT_NONE;
}

// 输入按下/松开状态，生成边沿和多击候选事件。
void KeyGesture_SetPressed(KeyGesture_t *gesture, uint8_t pressed, uint32_t now)
{
    if (gesture == 0) {
        return;
    }
    pressed = (pressed != 0U) ? 1U : 0U;
    if (gesture->pressed == pressed) {
        return;
    }

    gesture->pressed = pressed;
    if (pressed != 0U) {
        gesture->press_start_tick = now;
        gesture->long_reported = 0U;
        gesture->events |= KEY_EVENT_PRESS;
        return;
    }

    gesture->events |= KEY_EVENT_RELEASE;
    if (gesture->long_reported != 0U) {
        gesture->events |= KEY_EVENT_LONG_RELEASE;
        gesture->click_count = 0U;
        return;
    }

    gesture->click_count++;
    gesture->click_tick = now;
    if (gesture->click_count == 2U) {
        gesture->events |= KEY_EVENT_DOUBLE_CLICK;
    } else if (gesture->click_count >= 3U) {
        gesture->events |= KEY_EVENT_TRIPLE_CLICK;
        gesture->click_count = 0U;
    }
}

// 推进长按和单击超时，并取走本次事件。
uint16_t KeyGesture_Task(KeyGesture_t *gesture, uint32_t now)
{
    uint16_t events;

    if (gesture == 0) {
        return KEY_EVENT_NONE;
    }
    if ((gesture->pressed != 0U) &&
        (gesture->long_reported == 0U) &&
        ((uint32_t)(now - gesture->press_start_tick) >= KEY_LONG_PRESS_TIME_MS)) {
        gesture->long_reported = 1U;
        gesture->click_count = 0U;
        gesture->events |= KEY_EVENT_LONG_START;
    }
    if ((gesture->click_count != 0U) &&
        ((uint32_t)(now - gesture->click_tick) > KEY_MULTI_CLICK_TIME_MS)) {
        if (gesture->click_count == 1U) {
            gesture->events |= KEY_EVENT_SINGLE_CLICK;
        }
        gesture->click_count = 0U;
    }

    events = gesture->events;
    gesture->events = KEY_EVENT_NONE;
    return events;
}

// 查找有效按键管理器。
static KeyManager_t *Key_GetManager(KeyCode_t code)
{
    if ((code == Key_None) || ((uint8_t)code >= (uint8_t)Key_TableSize)) {
        return 0;
    }
    if (s_key_table[(uint8_t)code].config.enable == 0U) {
        return 0;
    }
    return &s_key_table[(uint8_t)code];
}

// 处理一个按键的消抖、单/双/三击和长按事件。
static void Key_ProcessOne(KeyManager_t *key, uint32_t now)
{
    uint8_t raw;

    if ((key == 0) || (key->config.enable == 0U)) {
        return;
    }

    raw = Key_ReadRawPressed(key);
    if (raw != key->runtime.raw_pressed) {
        key->runtime.raw_pressed = raw;
        key->runtime.last_change_tick = now;
        return;
    }
    if ((uint32_t)(now - key->runtime.last_change_tick) < KEY_DEBOUNCE_TIME_MS) {
        return;
    }

    if (key->runtime.stable_pressed != raw) {
        key->runtime.stable_pressed = raw;
        if (raw != 0U) {
            key->runtime.press_start_tick = now;
            key->runtime.long_reported = 0U;
            key->runtime.events |= KEY_EVENT_PRESS;
        } else {
            key->runtime.events |= KEY_EVENT_RELEASE;
            if (key->runtime.long_reported != 0U) {
                key->runtime.events |= KEY_EVENT_LONG_RELEASE;
                key->runtime.click_count = 0U;
            } else {
                key->runtime.click_count++;
                key->runtime.click_tick = now;
                if (key->runtime.click_count == 2U) {
                    key->runtime.events |= KEY_EVENT_DOUBLE_CLICK;
                } else if (key->runtime.click_count >= 3U) {
                    key->runtime.events |= KEY_EVENT_TRIPLE_CLICK;
                    key->runtime.click_count = 0U;
                }
            }

            if (key->config.trigger_mode == KEY_TRIGGER_CLICK) {
                key->flag = (key->flag == 0U) ? 1U : 0U;
            } else {
                key->flag = 0U;
            }
        }
    }

    if ((key->runtime.stable_pressed != 0U) &&
        (key->runtime.long_reported == 0U) &&
        ((uint32_t)(now - key->runtime.press_start_tick) >= KEY_LONG_PRESS_TIME_MS)) {
        key->runtime.long_reported = 1U;
        key->runtime.click_count = 0U;
        key->runtime.events |= KEY_EVENT_LONG_START;
        if (key->config.trigger_mode == KEY_TRIGGER_LONG_PRESS) {
            key->flag = 1U;
        }
    }

    if ((key->runtime.click_count != 0U) &&
        ((uint32_t)(now - key->runtime.click_tick) > KEY_MULTI_CLICK_TIME_MS)) {
        if (key->runtime.click_count == 1U) {
            key->runtime.events |= KEY_EVENT_SINGLE_CLICK;
        }
        key->runtime.click_count = 0U;
    }
}

// 读取 GPIO 有效按下状态。
static uint8_t Key_ReadRawPressed(KeyManager_t *key)
{
    GPIO_PinState state;
    if ((key == 0) || (key->config.port == 0)) {
        return 0U;
    }
    state = HAL_GPIO_ReadPin(key->config.port, key->config.pin);
    return Input_Resolve((uint8_t)key->code,(uint8_t)(state==key->config.is_pressed),HAL_GetTick());
}