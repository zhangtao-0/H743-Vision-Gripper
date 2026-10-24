/*********************************************************************************************************************
 * @file    claw_limit_storage.h
 * @brief   夹爪机械极限 Flash 持久化接口。
 *********************************************************************************************************************/

#ifndef __CLAW_LIMIT_STORAGE_H__
#define __CLAW_LIMIT_STORAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include <stdint.h>


typedef struct {
    uint16_t open_position;
    uint16_t close_position;
    uint32_t sequence;
    uint8_t limits_valid;
    uint8_t sweep_range;
} ClawLimitStorageData_t;

HAL_StatusTypeDef ClawLimitStorage_Load(ClawLimitStorageData_t *out_data);
HAL_StatusTypeDef ClawLimitStorage_Save(uint16_t open_position, uint16_t close_position);
HAL_StatusTypeDef ClawLimitStorage_SaveConfig(uint16_t open_position,
                                               uint16_t close_position,
                                               uint8_t limits_valid,
                                               uint8_t sweep_range);
HAL_StatusTypeDef ClawLimitStorage_Clear(void);

#ifdef __cplusplus
}
#endif

#endif
