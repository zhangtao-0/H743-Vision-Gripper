#ifndef TARGET_CONFIG_H
#define TARGET_CONFIG_H
/* Override this one macro with a target header when adding another board/MCU. */
#ifndef APP_TARGET_CONFIG_HEADER
#define APP_TARGET_CONFIG_HEADER "targets/stm32h743_lxb.h"
#endif
#include APP_TARGET_CONFIG_HEADER

#endif
