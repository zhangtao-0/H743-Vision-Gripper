/*********************************************************************************************************************
 * @file    screen_display.h
 * @brief   OLED 状态页显示接口。
 *
 * @details
 * 1. 本文件负责：声明状态页初始化和周期刷新入口。
 * 2. 本文件不负责：暴露 OLED 底层时序。
 * 3. 主要流程：控制层更新 -> Screen_Task 周期显示。
 *********************************************************************************************************************/

#ifndef __SCREEN_DISPLAY_H__
#define __SCREEN_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include <stdint.h>

#ifndef SCREEN_ENABLE_OLED
#define SCREEN_ENABLE_OLED                    1U      /* 1：启用 OLED 显示；0：编译保留接口但不刷新屏幕。 */
#endif

#ifndef SCREEN_REFRESH_PERIOD_MS
#define SCREEN_REFRESH_PERIOD_MS              200U    /* OLED 刷新周期，单位 ms。 */
#endif

void Screen_Init(void);
void Screen_Task(void);

#ifdef __cplusplus
}
#endif

#endif
