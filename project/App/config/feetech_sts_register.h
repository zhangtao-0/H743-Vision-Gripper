/*********************************************************************************************************************
 * @file    feetech_sts_register.h
 * @brief   飞特 STS 官方寄存器描述表接口。
 *********************************************************************************************************************/

#ifndef __FEETECH_STS_REGISTER_H__
#define __FEETECH_STS_REGISTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    FEETECH_STS_REG_STORAGE_EPROM = 0,
    FEETECH_STS_REG_STORAGE_SRAM,
    FEETECH_STS_REG_STORAGE_DEFAULT
} FeetechStsRegisterStorage_t;

typedef enum {
    FEETECH_STS_REG_ACCESS_RO = 0,
    FEETECH_STS_REG_ACCESS_RW
} FeetechStsRegisterAccess_t;

typedef enum {
    FEETECH_STS_REG_VALUE_U8 = 0,
    FEETECH_STS_REG_VALUE_U16,
    FEETECH_STS_REG_VALUE_S16,
    FEETECH_STS_REG_VALUE_BITFIELD,
    FEETECH_STS_REG_VALUE_RAW
} FeetechStsRegisterValueType_t;

typedef enum {
    FEETECH_STS_REG_UNIT_RAW = 0,
    FEETECH_STS_REG_UNIT_STEP,
    FEETECH_STS_REG_UNIT_STEP_PER_S,
    FEETECH_STS_REG_UNIT_VOLT_01V,
    FEETECH_STS_REG_UNIT_CURRENT_65MA,
    FEETECH_STS_REG_UNIT_CELSIUS,
    FEETECH_STS_REG_UNIT_PERCENT_001,
    FEETECH_STS_REG_UNIT_PERCENT_01,
    FEETECH_STS_REG_UNIT_MS,
    FEETECH_STS_REG_UNIT_10MS,
    FEETECH_STS_REG_UNIT_2US,
    FEETECH_STS_REG_UNIT_ACC_100_STEP_PER_S2
} FeetechStsRegisterUnit_t;

typedef enum {
    FEETECH_STS_REG_CHECK_OK = 0,
    FEETECH_STS_REG_CHECK_INVALID,
    FEETECH_STS_REG_CHECK_UNKNOWN,
    FEETECH_STS_REG_CHECK_PARTIAL,
    FEETECH_STS_REG_CHECK_READ_ONLY,
    FEETECH_STS_REG_CHECK_DISABLED,
    FEETECH_STS_REG_CHECK_DANGEROUS,
    FEETECH_STS_REG_CHECK_RANGE
} FeetechStsRegisterCheckResult_t;

typedef struct {
    uint8_t address;
    uint8_t width;
    FeetechStsRegisterStorage_t storage;
    FeetechStsRegisterAccess_t access;
    FeetechStsRegisterValueType_t value_type;
    FeetechStsRegisterUnit_t unit;
    int32_t min_value;
    int32_t max_value;
    uint8_t write_enable;
    uint8_t dangerous;
    const char *name;
} FeetechStsRegisterDesc_t;

// 按寄存器起始地址查找。
const FeetechStsRegisterDesc_t *FeetechStsRegister_FindByStart(uint8_t address);

// 查找覆盖指定字节的寄存器。
const FeetechStsRegisterDesc_t *FeetechStsRegister_FindContaining(uint8_t address);

// 校验连续写入范围、权限和数值。
FeetechStsRegisterCheckResult_t FeetechStsRegister_CheckWriteRange(
    uint8_t start_address,
    const uint8_t *data,
    uint8_t data_len,
    const FeetechStsRegisterDesc_t **failed_desc,
    int32_t *failed_value);

// 按描述表解码小端值。
uint8_t FeetechStsRegister_DecodeValue(
    const FeetechStsRegisterDesc_t *desc,
    const uint8_t *data,
    uint8_t data_len,
    int32_t *value);

// 判断整个地址范围是否均有描述。
uint8_t FeetechStsRegister_IsKnownRange(uint8_t start_address, uint8_t data_len);

#ifdef __cplusplus
}
#endif

#endif