/*********************************************************************************************************************
 * @file    screen_display.c
 * @brief   OLED 状态页显示实现。
 *********************************************************************************************************************/

#include "screen_display.h"
#include "OLED.h"
#include "feetech_sts_control.h"

static uint32_t s_screen_last_refresh_tick;

static void Screen_DrawTemplate(void);
static void Screen_ShowFixedText(uint8_t line, uint8_t column, const char *text, uint8_t width);
static uint16_t Screen_FloatToU16(float value);

// 初始化 OLED 状态页。
void Screen_Init(void)
{
#if (SCREEN_ENABLE_OLED != 0U)
    s_screen_last_refresh_tick = 0U;
    OLED_Init();
    OLED_Clear();
    Screen_DrawTemplate();
#else
    s_screen_last_refresh_tick = 0U;
#endif
}

// 刷新位置、目标、电压和电流。
void Screen_Task(void)
{
#if (SCREEN_ENABLE_OLED != 0U)
    uint32_t now_tick;
    const FeetechStsManager_t *manager;
    uint16_t voltage_mv;
    uint16_t actual_angle;
    uint16_t target_angle;
    uint16_t current_ma;
    uint16_t min_angle;
    uint16_t max_angle;

    now_tick = HAL_GetTick();
    if ((uint32_t)(now_tick - s_screen_last_refresh_tick) < SCREEN_REFRESH_PERIOD_MS) {
        return;
    }
    s_screen_last_refresh_tick = now_tick;

    manager = FeetechStsControl_GetManager();
    if (manager == 0) {
        return;
    }

    voltage_mv = manager->feedback.voltage_mv;
    actual_angle = Screen_FloatToU16(manager->feedback.angle_deg);
    target_angle = Screen_FloatToU16(manager->target.target_angle_deg);
    current_ma = manager->feedback.current_ma;
    min_angle = Screen_FloatToU16(FeetechStsControl_GetMotionMinAngle());
    max_angle = Screen_FloatToU16(FeetechStsControl_GetMotionMaxAngle());

    OLED_ShowString(1U, 1U, "A:");
    if (manager->feedback.position_valid != 0U) {
        OLED_ShowNum(1U, 3U, actual_angle, 3U);
    } else {
        OLED_ShowString(1U, 3U, "---");
    }
    OLED_ShowString(1U, 6U, " TG:");
    OLED_ShowNum(1U, 10U, target_angle, 3U);
    OLED_ShowString(1U, 13U, "    ");

    OLED_ShowString(2U, 1U, "V:");
    if (manager->feedback.telemetry_valid != 0U) {
        OLED_ShowNum(2U, 3U, voltage_mv, 5U);
    } else {
        OLED_ShowString(2U, 3U, "---- ");
    }
    OLED_ShowString(2U, 8U, " I:");
    if (manager->feedback.telemetry_valid != 0U) {
        OLED_ShowNum(2U, 11U, current_ma, 4U);
    } else {
        OLED_ShowString(2U, 11U, "----");
    }
    OLED_ShowString(2U, 15U, "  ");

    OLED_ShowString(3U, 1U, "MN:");
    OLED_ShowNum(3U, 4U, min_angle, 3U);
    OLED_ShowString(3U, 7U, " MX:");
    OLED_ShowNum(3U, 11U, max_angle, 3U);
    OLED_ShowString(3U, 14U, "   ");

    OLED_ShowString(4U, 1U, "ST:");
    Screen_ShowFixedText(4U, 4U, FeetechStsControl_GetStateText(), 4U);
    OLED_ShowString(4U, 8U, " SRC:");
    Screen_ShowFixedText(4U, 13U, FeetechStsControl_GetOwnerText(), 4U);
#endif
}

// 绘制 OLED 固定模板。
static void Screen_DrawTemplate(void)
{
    OLED_ShowString(1U, 1U, "A:--- TG:---   ");
    OLED_ShowString(2U, 1U, "V:----  I:---- ");
    OLED_ShowString(3U, 1U, "MN:--- MX:---   ");
    OLED_ShowString(4U, 1U, "ST:---- SRC:----");
}

// 定宽显示文本并覆盖旧字符。
static void Screen_ShowFixedText(uint8_t line, uint8_t column, const char *text, uint8_t width)
{
    uint8_t i;
    char buf[8];

    if (width >= sizeof(buf)) {
        width = (uint8_t)(sizeof(buf) - 1U);
    }
    for (i = 0U; i < width; i++) {
        buf[i] = ((text != 0) && (text[i] != '\0')) ? text[i] : ' ';
    }
    buf[width] = '\0';
    OLED_ShowString(line, column, buf);
}

// 浮点角度转换为显示整数。
static uint16_t Screen_FloatToU16(float value)
{
    if (value <= 0.0f) {
        return 0U;
    }
    if (value >= 999.0f) {
        return 999U;
    }
    return (uint16_t)(value + 0.5f);
}