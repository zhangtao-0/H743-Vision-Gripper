#ifndef PLATFORM_HAL_H
#define PLATFORM_HAL_H
#include "target_config.h"
#if PLATFORM_MCU_FAMILY_H7 && !defined(STM32H743xx)
#error "Selected H743 target requires STM32H743xx; another MCU needs its own target/board/backend."
#endif
#if defined(STM32H743xx) && defined(STM32F103xB)
#error "Multiple MCU targets selected"
#endif
#include PLATFORM_HAL_HEADER

#endif
