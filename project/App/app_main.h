/*********************************************************************************************************************
 * @file    app_main.h
 * @brief   应用层初始化与周期任务入口。
 *********************************************************************************************************************/

#ifndef APP_MAIN_H
#define APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

void App_Init(void);
void App_Task(void);
unsigned char App_MotionReady(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
