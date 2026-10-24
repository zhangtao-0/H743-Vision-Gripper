#ifndef UI_PORT_H
#define UI_PORT_H
#include "ui_core.h"
typedef enum {UI_ACTION_SCREEN,UI_ACTION_HOST,UI_ACTION_USB} UiActionSource;
UiResult UiPort_RunCommand(uint8_t id,UiActionSource source);
const UiOperations *UiPort_Operations(void);
uint32_t UiPort_ViewFlags(void);
void UiPort_ApplyViewFlags(uint32_t flags);
#endif
