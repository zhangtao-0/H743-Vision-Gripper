/*********************************************************************************************************************
 * @file    feetech_sts_register.c
 * @brief   飞特 STS 官方寄存器唯一描述表。
 *********************************************************************************************************************/

#include "feetech_sts_register.h"
#include "feature_config.h"
#include <stddef.h>

#define STS_REG(addr, bytes, area, rw, type, unit_type, min_v, max_v, enabled, risk, text) \
    { (addr), (bytes), (area), (rw), (type), (unit_type), (min_v), (max_v), (enabled), (risk), (text) }

// 地址、权限、范围和单位只在此表维护。
static const FeetechStsRegisterDesc_t s_registers[] = {
    STS_REG( 0U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     255, 0U, 0U, "固件主版本号"),
    STS_REG( 1U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     255, 0U, 0U, "固件次版本号"),
    STS_REG( 3U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     255, 0U, 0U, "舵机主版本号"),
    STS_REG( 4U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     255, 0U, 0U, "舵机次版本号"),
    STS_REG( 5U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     253, 0U, 1U, "ID"),
    STS_REG( 6U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,       7, 0U, 1U, "波特率"),
    STS_REG( 7U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_2US,                 0,     254, 1U, 0U, "返回延时"),
    STS_REG( 8U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,       1, 1U, 0U, "应答状态级别"),
    STS_REG( 9U, 2U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_STEP,                0,    4094, 1U, 0U, "最小角度限制"),
    STS_REG(11U, 2U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_STEP,                1,    4095, 1U, 0U, "最大角度限制"),
    STS_REG(13U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_CELSIUS,             0,     100, 1U, 0U, "最高温度上限"),
    STS_REG(14U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_VOLT_01V,            0,     CLAW_FEATURE_STS_MAX_VOLTAGE_RAW, CLAW_FEATURE_STS_MAX_VOLTAGE_WRITE_ENABLE, 1U, "最高输入电压"),
    STS_REG(15U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_VOLT_01V,            0,     254, 0U, 1U, "最低输入电压"),
    STS_REG(16U, 2U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_PERCENT_001,         0,    1000, 1U, 0U, "最大扭矩"),
    STS_REG(18U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_BITFIELD, FEETECH_STS_REG_UNIT_RAW,                 0,     254, 0U, 1U, "相位特殊字节"),
    STS_REG(19U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_BITFIELD, FEETECH_STS_REG_UNIT_RAW,                 0,     254, 0U, 1U, "卸载条件"),
    STS_REG(20U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_BITFIELD, FEETECH_STS_REG_UNIT_RAW,                 0,     254, 1U, 0U, "LED报警条件"),
    STS_REG(21U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     254, 1U, 0U, "位置环P比例系数"),
    STS_REG(22U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     254, 1U, 0U, "位置环D微分系数"),
    STS_REG(23U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     254, 1U, 0U, "位置环I积分系数"),
    STS_REG(24U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_PERCENT_001,         0,     254, 1U, 0U, "最小启动力"),
    STS_REG(25U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     254, 1U, 0U, "积分限制值"),
    STS_REG(26U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_STEP,                0,      32, 1U, 0U, "顺时针不灵敏区"),
    STS_REG(27U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_STEP,                0,      32, 1U, 0U, "逆时针不灵敏区"),
    STS_REG(28U, 2U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_CURRENT_65MA,        0,     511, 1U, 0U, "保护电流"),
    STS_REG(30U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 1,       3, 0U, 1U, "角度分辨率"),
    STS_REG(31U, 2U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_S16,      FEETECH_STS_REG_UNIT_STEP,            -2047,    2047, 0U, 1U, "位置校正"),
    STS_REG(33U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,       3, 1U, 0U, "运行模式"),
    STS_REG(34U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_PERCENT_01,          0,     100, 1U, 0U, "保护扭矩"),
    STS_REG(35U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_10MS,                0,     254, 1U, 0U, "保护时间"),
    STS_REG(36U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_PERCENT_01,          0,     100, 1U, 0U, "过载扭矩"),
    STS_REG(37U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     254, 1U, 0U, "速度闭环P比例系数"),
    STS_REG(38U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_10MS,                0,     254, 1U, 0U, "过流保护时间"),
    STS_REG(39U, 1U, FEETECH_STS_REG_STORAGE_EPROM, FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     254, 1U, 0U, "速度闭环I积分系数"),
    STS_REG(40U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     128, 1U, 0U, "扭矩开关"),
    STS_REG(41U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_ACC_100_STEP_PER_S2, 0,     254, 1U, 0U, "加速度"),
    STS_REG(42U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_STEP,                0,    4095, 1U, 0U, "目标位置"),
    STS_REG(44U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_MS,                  0,    1000, 1U, 0U, "运行时间"),
    STS_REG(46U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_STEP_PER_S,          0,    4095, 1U, 0U, "运行速度"),
    STS_REG(48U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_PERCENT_001,         0,    1000, 1U, 0U, "转矩限制"),
    STS_REG(55U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RW, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,       1, 1U, 0U, "锁标志"),
    STS_REG(56U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U16,      FEETECH_STS_REG_UNIT_STEP,                0,    4095, 0U, 0U, "当前位置"),
    STS_REG(58U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_S16,      FEETECH_STS_REG_UNIT_STEP_PER_S,     -32768,   32767, 0U, 0U, "当前速度"),
    STS_REG(60U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_S16,      FEETECH_STS_REG_UNIT_PERCENT_001,    -32768,   32767, 0U, 0U, "当前负载"),
    STS_REG(62U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_VOLT_01V,            0,     255, 0U, 0U, "当前电压"),
    STS_REG(63U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_CELSIUS,             0,     255, 0U, 0U, "当前温度"),
    STS_REG(64U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     255, 0U, 0U, "异步写标志"),
    STS_REG(65U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_BITFIELD, FEETECH_STS_REG_UNIT_RAW,                 0,     255, 0U, 0U, "舵机状态"),
    STS_REG(66U, 1U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_U8,       FEETECH_STS_REG_UNIT_RAW,                 0,     255, 0U, 0U, "移动标志"),
    STS_REG(69U, 2U, FEETECH_STS_REG_STORAGE_SRAM,  FEETECH_STS_REG_ACCESS_RO, FEETECH_STS_REG_VALUE_S16,      FEETECH_STS_REG_UNIT_CURRENT_65MA,   -32768,   32767, 0U, 0U, "当前电流")
};

#define FEETECH_STS_REGISTER_COUNT ((uint8_t)(sizeof(s_registers) / sizeof(s_registers[0])))

// 按起始地址查找描述。
const FeetechStsRegisterDesc_t *FeetechStsRegister_FindByStart(uint8_t address)
{
    uint8_t i;

    for (i = 0U; i < FEETECH_STS_REGISTER_COUNT; i++) {
        if (s_registers[i].address == address) {
            return &s_registers[i];
        }
    }
    return 0;
}

// 查找覆盖当前字节的描述。
const FeetechStsRegisterDesc_t *FeetechStsRegister_FindContaining(uint8_t address)
{
    uint8_t i;
    uint16_t end;

    for (i = 0U; i < FEETECH_STS_REGISTER_COUNT; i++) {
        end = (uint16_t)s_registers[i].address + (uint16_t)s_registers[i].width;
        if (((uint16_t)address >= (uint16_t)s_registers[i].address) &&
            ((uint16_t)address < end)) {
            return &s_registers[i];
        }
    }
    return 0;
}

// 按小端格式解码寄存器值。
uint8_t FeetechStsRegister_DecodeValue(const FeetechStsRegisterDesc_t *desc,
                                       const uint8_t *data,
                                       uint8_t data_len,
                                       int32_t *value)
{
    uint16_t raw;

    if ((desc == 0) || (data == 0) || (value == 0) ||
        (desc->width == 0U) || (data_len < desc->width)) {
        return 0U;
    }

    if (desc->width == 1U) {
        *value = data[0];
        return 1U;
    }
    if (desc->width != 2U) {
        return 0U;
    }

    raw = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    if (desc->value_type == FEETECH_STS_REG_VALUE_S16) {
        *value = (int16_t)raw;
    } else {
        *value = raw;
    }
    return 1U;
}

// 校验连续写入：地址边界 -> 权限 -> 开关 -> 数值范围。
FeetechStsRegisterCheckResult_t FeetechStsRegister_CheckWriteRange(
    uint8_t start_address,
    const uint8_t *data,
    uint8_t data_len,
    const FeetechStsRegisterDesc_t **failed_desc,
    int32_t *failed_value)
{
    const FeetechStsRegisterDesc_t *desc;
    uint16_t offset;
    uint16_t address;
    int32_t value;

    if (failed_desc != 0) {
        *failed_desc = 0;
    }
    if (failed_value != 0) {
        *failed_value = 0;
    }
    if ((data == 0) || (data_len == 0U) ||
        (((uint16_t)start_address + (uint16_t)data_len) > 256U)) {
        return FEETECH_STS_REG_CHECK_INVALID;
    }

    offset = 0U;
    while (offset < data_len) {
        address = (uint16_t)start_address + offset;
        desc = FeetechStsRegister_FindByStart((uint8_t)address);
        if (desc == 0) {
            desc = FeetechStsRegister_FindContaining((uint8_t)address);
            if (desc != 0) {
                if (failed_desc != 0) {
                    *failed_desc = desc;
                }
                return FEETECH_STS_REG_CHECK_PARTIAL;
            }
#if (CLAW_FEATURE_STS_UNKNOWN_WRITE_ENABLE != 0U)
            offset++;
            continue;
#else
            return FEETECH_STS_REG_CHECK_UNKNOWN;
#endif
        }

        if (failed_desc != 0) {
            *failed_desc = desc;
        }
        if ((offset + desc->width) > data_len) {
            return FEETECH_STS_REG_CHECK_PARTIAL;
        }
        if (desc->access != FEETECH_STS_REG_ACCESS_RW) {
            return FEETECH_STS_REG_CHECK_READ_ONLY;
        }
        if (desc->write_enable == 0U) {
            if (desc->dangerous != 0U) {
#if (CLAW_FEATURE_STS_DANGEROUS_WRITE_ENABLE == 0U)
                return FEETECH_STS_REG_CHECK_DANGEROUS;
#endif
            } else {
                return FEETECH_STS_REG_CHECK_DISABLED;
            }
        }
        if (FeetechStsRegister_DecodeValue(desc, &data[offset], (uint8_t)(data_len - offset), &value) == 0U) {
            return FEETECH_STS_REG_CHECK_INVALID;
        }
        if (failed_value != 0) {
            *failed_value = value;
        }
        if ((value < desc->min_value) || (value > desc->max_value)) {
            return FEETECH_STS_REG_CHECK_RANGE;
        }
        if ((desc->address == 40U) && (value != 0) && (value != 1) && (value != 128)) {
            return FEETECH_STS_REG_CHECK_RANGE;
        }
        offset += desc->width;
    }

    if (failed_desc != 0) {
        *failed_desc = 0;
    }
    return FEETECH_STS_REG_CHECK_OK;
}

// 判断范围内每个字节是否都有寄存器描述。
uint8_t FeetechStsRegister_IsKnownRange(uint8_t start_address, uint8_t data_len)
{
    uint16_t i;
    uint16_t address;

    if ((data_len == 0U) || (((uint16_t)start_address + (uint16_t)data_len) > 256U)) {
        return 0U;
    }
    for (i = 0U; i < data_len; i++) {
        address = (uint16_t)start_address + i;
        if (FeetechStsRegister_FindContaining((uint8_t)address) == 0) {
            return 0U;
        }
    }
    return 1U;
}