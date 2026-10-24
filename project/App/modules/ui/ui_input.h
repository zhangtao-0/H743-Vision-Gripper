#ifndef UI_INPUT_H
#define UI_INPUT_H
#include "ui_core.h"
typedef struct {uint8_t previous[4],chord;} UiInput;
void UiInput_Init(UiInput *);
UiEvent UiInput_Sample(UiInput *,uint32_t now,uint8_t up,uint8_t down,uint8_t ok,uint8_t back);
#endif
