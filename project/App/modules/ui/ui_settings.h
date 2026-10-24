#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H
#include "ui_core.h"
void UiSettings_Init(void);
void UiSettings_ApplySts(void);
UiResult UiSettings_Save(void);
UiResult UiSettings_Load(void);
UiResult UiSettings_Clear(void);
uint8_t UiSettings_HasProfile(void);
uint32_t UiSettings_Sequence(void);
#endif
