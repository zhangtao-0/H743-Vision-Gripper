/*********************************************************************************************************************
 * @file    claw_limit_storage_ram.c
 * @brief   P1 临时存储后端：把夹爪限位记录保存在 RAM。
 *
 * @details
 * 1. H743 的 Flash 扇区/地址与 F103 完全不同，旧的后端不能直接沿用。
 * 2. P1 先提供与 claw_limit_storage.h 完全相同的 API，保证控制层能编译、能跑业务回归。
 * 3. P4 再实现 H7 Flash 保留区后端；切换时只替换本文件，不改调用者。
 *********************************************************************************************************************/

#include "claw_limit_storage.h"

static ClawLimitStorageData_t s_storage_data;
static uint8_t s_storage_valid;

HAL_StatusTypeDef ClawLimitStorage_Load(ClawLimitStorageData_t *out_data)
{
    if (out_data == 0) {
        return HAL_ERROR;
    }
    if (s_storage_valid == 0U) {
        return HAL_ERROR;
    }

    *out_data = s_storage_data;
    return HAL_OK;
}

HAL_StatusTypeDef ClawLimitStorage_Save(uint16_t open_position,
                                        uint16_t close_position)
{
    return ClawLimitStorage_SaveConfig(open_position, close_position, 1U, 0U);
}

HAL_StatusTypeDef ClawLimitStorage_SaveConfig(uint16_t open_position,
                                              uint16_t close_position,
                                              uint8_t limits_valid,
                                              uint8_t sweep_range)
{
    s_storage_data.open_position = open_position;
    s_storage_data.close_position = close_position;
    s_storage_data.sequence++;
    s_storage_data.limits_valid = limits_valid;
    s_storage_data.sweep_range = sweep_range;
    s_storage_valid = 1U;

    return HAL_OK;
}

HAL_StatusTypeDef ClawLimitStorage_Clear(void)
{
    s_storage_valid = 0U;
    s_storage_data.open_position = 0U;
    s_storage_data.close_position = 0U;
    s_storage_data.sequence = 0U;
    s_storage_data.limits_valid = 0U;
    s_storage_data.sweep_range = 0U;

    return HAL_OK;
}
