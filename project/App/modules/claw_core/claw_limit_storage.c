/*********************************************************************************************************************
 * @file    claw_limit_storage.c
 * @brief   夹爪机械极限和行程模式 Flash 双页持久化实现。
 *********************************************************************************************************************/

#include "claw_limit_storage.h"
#include <stddef.h>
#include <string.h>

#define CLAW_LIMIT_STORAGE_MAGIC              0x434C4157UL
#define CLAW_LIMIT_STORAGE_VERSION_V1         1U
#define CLAW_LIMIT_STORAGE_VERSION_V2         2U
#define CLAW_LIMIT_STORAGE_COMMIT             0xA55AC33CUL
#define CLAW_LIMIT_STORAGE_POSITION_MAX       4095U
#define CLAW_LIMIT_STORAGE_RANGE_MAX          1U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint16_t open_position;
    uint16_t close_position;
    uint32_t checksum;
    uint32_t commit;
} ClawLimitStorageRecordV1_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint16_t open_position;
    uint16_t close_position;
    uint8_t limits_valid;
    uint8_t sweep_range;
    uint16_t reserved;
    uint32_t checksum;
    uint32_t commit;
} ClawLimitStorageRecordV2_t;

typedef char ClawLimitStorageRecordSizeMustBeEven[(sizeof(ClawLimitStorageRecordV2_t) % 2U) == 0U ? 1 : -1];

static uint32_t ClawLimitStorage_Checksum(const void *record, uint32_t length);
static uint8_t ClawLimitStorage_ReadRecord(uint32_t address, ClawLimitStorageData_t *out_data);
static HAL_StatusTypeDef ClawLimitStorage_ErasePage(uint32_t address);
static HAL_StatusTypeDef ClawLimitStorage_ProgramRecord(uint32_t address, const ClawLimitStorageRecordV2_t *record);

// 读取序号最新的有效配置，V1 自动按“机械极限模式”解释。
HAL_StatusTypeDef ClawLimitStorage_Load(ClawLimitStorageData_t *out_data)
{
    ClawLimitStorageData_t data_a;
    ClawLimitStorageData_t data_b;
    uint8_t valid_a;
    uint8_t valid_b;

    if (out_data == 0) {
        return HAL_ERROR;
    }

    valid_a = ClawLimitStorage_ReadRecord(CLAW_LIMIT_STORAGE_PAGE_A_ADDRESS, &data_a);
    valid_b = ClawLimitStorage_ReadRecord(CLAW_LIMIT_STORAGE_PAGE_B_ADDRESS, &data_b);
    if ((valid_a == 0U) && (valid_b == 0U)) {
        memset(out_data, 0, sizeof(*out_data));
        return HAL_ERROR;
    }

    if ((valid_a != 0U) && (valid_b != 0U)) {
        *out_data = (data_a.sequence >= data_b.sequence) ? data_a : data_b;
    } else {
        *out_data = (valid_a != 0U) ? data_a : data_b;
    }
    return HAL_OK;
}

// 兼容接口：保存有效机械极限，并选择机械极限行程。
HAL_StatusTypeDef ClawLimitStorage_Save(uint16_t open_position, uint16_t close_position)
{
    return ClawLimitStorage_SaveConfig(open_position, close_position, 1U, 0U);
}

// 保存机械极限和行程模式：保留旧页 -> 写新页 -> 最后写提交标记。
HAL_StatusTypeDef ClawLimitStorage_SaveConfig(uint16_t open_position,
                                               uint16_t close_position,
                                               uint8_t limits_valid,
                                               uint8_t sweep_range)
{
    ClawLimitStorageData_t data_a;
    ClawLimitStorageData_t data_b;
    ClawLimitStorageRecordV2_t record;
    uint32_t target_address;
    uint32_t sequence;
    uint8_t valid_a;
    uint8_t valid_b;
    HAL_StatusTypeDef status;

    limits_valid = (limits_valid != 0U) ? 1U : 0U;
    if ((sweep_range > CLAW_LIMIT_STORAGE_RANGE_MAX) ||
        ((limits_valid != 0U) &&
         ((open_position > CLAW_LIMIT_STORAGE_POSITION_MAX) ||
          (close_position > CLAW_LIMIT_STORAGE_POSITION_MAX) ||
          (open_position == close_position)))) {
        return HAL_ERROR;
    }

    valid_a = ClawLimitStorage_ReadRecord(CLAW_LIMIT_STORAGE_PAGE_A_ADDRESS, &data_a);
    valid_b = ClawLimitStorage_ReadRecord(CLAW_LIMIT_STORAGE_PAGE_B_ADDRESS, &data_b);
    sequence = 1UL;
    if ((valid_a != 0U) || (valid_b != 0U)) {
        if ((valid_a != 0U) && ((valid_b == 0U) || (data_a.sequence >= data_b.sequence))) {
            sequence = data_a.sequence + 1UL;
            target_address = CLAW_LIMIT_STORAGE_PAGE_B_ADDRESS;
        } else {
            sequence = data_b.sequence + 1UL;
            target_address = CLAW_LIMIT_STORAGE_PAGE_A_ADDRESS;
        }
    } else {
        target_address = CLAW_LIMIT_STORAGE_PAGE_A_ADDRESS;
    }
    if (sequence == 0UL) {
        sequence = 1UL;
    }

    memset(&record, 0xFF, sizeof(record));
    record.magic = CLAW_LIMIT_STORAGE_MAGIC;
    record.version = CLAW_LIMIT_STORAGE_VERSION_V2;
    record.size = (uint16_t)sizeof(record);
    record.sequence = sequence;
    record.open_position = open_position;
    record.close_position = close_position;
    record.limits_valid = limits_valid;
    record.sweep_range = sweep_range;
    record.reserved = 0xFFFFU;
    record.commit = CLAW_LIMIT_STORAGE_COMMIT;
    record.checksum = ClawLimitStorage_Checksum(&record, (uint32_t)offsetof(ClawLimitStorageRecordV2_t, checksum));

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return status;
    }
    status = ClawLimitStorage_ErasePage(target_address);
    if (status == HAL_OK) {
        status = ClawLimitStorage_ProgramRecord(target_address, &record);
    }
    (void)HAL_FLASH_Lock();
    if (status != HAL_OK) {
        return status;
    }

    return ClawLimitStorage_ReadRecord(target_address, &data_a) ? HAL_OK : HAL_ERROR;
}

// 清除两页机械极限和行程模式。
HAL_StatusTypeDef ClawLimitStorage_Clear(void)
{
    HAL_StatusTypeDef status;

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return status;
    }
    status = ClawLimitStorage_ErasePage(CLAW_LIMIT_STORAGE_PAGE_A_ADDRESS);
    if (status == HAL_OK) {
        status = ClawLimitStorage_ErasePage(CLAW_LIMIT_STORAGE_PAGE_B_ADDRESS);
    }
    (void)HAL_FLASH_Lock();
    return status;
}

// 计算记录校验和。
static uint32_t ClawLimitStorage_Checksum(const void *record, uint32_t length)
{
    const uint8_t *bytes;
    uint32_t value;
    uint32_t i;

    bytes = (const uint8_t *)record;
    value = 2166136261UL;
    for (i = 0UL; i < length; i++) {
        value ^= bytes[i];
        value *= 16777619UL;
    }
    return value;
}

// 读取并校验单页 V1/V2 记录。
static uint8_t ClawLimitStorage_ReadRecord(uint32_t address, ClawLimitStorageData_t *out_data)
{
    const ClawLimitStorageRecordV1_t *v1;
    const ClawLimitStorageRecordV2_t *v2;

    if (out_data == 0) {
        return 0U;
    }
    v1 = (const ClawLimitStorageRecordV1_t *)address;
    if ((v1->magic == CLAW_LIMIT_STORAGE_MAGIC) &&
        (v1->version == CLAW_LIMIT_STORAGE_VERSION_V1) &&
        (v1->size == (uint16_t)sizeof(*v1)) &&
        (v1->commit == CLAW_LIMIT_STORAGE_COMMIT) &&
        (v1->open_position <= CLAW_LIMIT_STORAGE_POSITION_MAX) &&
        (v1->close_position <= CLAW_LIMIT_STORAGE_POSITION_MAX) &&
        (v1->open_position != v1->close_position) &&
        (v1->checksum == ClawLimitStorage_Checksum(v1, (uint32_t)offsetof(ClawLimitStorageRecordV1_t, checksum)))) {
        out_data->open_position = v1->open_position;
        out_data->close_position = v1->close_position;
        out_data->sequence = v1->sequence;
        out_data->limits_valid = 1U;
        out_data->sweep_range = 0U;
        return 1U;
    }

    v2 = (const ClawLimitStorageRecordV2_t *)address;
    if ((v2->magic != CLAW_LIMIT_STORAGE_MAGIC) ||
        (v2->version != CLAW_LIMIT_STORAGE_VERSION_V2) ||
        (v2->size != (uint16_t)sizeof(*v2)) ||
        (v2->commit != CLAW_LIMIT_STORAGE_COMMIT) ||
        (v2->sweep_range > CLAW_LIMIT_STORAGE_RANGE_MAX) ||
        (v2->checksum != ClawLimitStorage_Checksum(v2, (uint32_t)offsetof(ClawLimitStorageRecordV2_t, checksum)))) {
        return 0U;
    }
    if ((v2->limits_valid != 0U) &&
        ((v2->open_position > CLAW_LIMIT_STORAGE_POSITION_MAX) ||
         (v2->close_position > CLAW_LIMIT_STORAGE_POSITION_MAX) ||
         (v2->open_position == v2->close_position))) {
        return 0U;
    }

    out_data->open_position = v2->open_position;
    out_data->close_position = v2->close_position;
    out_data->sequence = v2->sequence;
    out_data->limits_valid = (v2->limits_valid != 0U) ? 1U : 0U;
    out_data->sweep_range = v2->sweep_range;
    return 1U;
}

// 擦除一页 Flash。
static HAL_StatusTypeDef ClawLimitStorage_ErasePage(uint32_t address)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error;

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = address;
    erase.NbPages = 1U;
    page_error = 0xFFFFFFFFUL;
    return HAL_FLASHEx_Erase(&erase, &page_error);
}

// 半字写入记录，提交标记最后写入。
static HAL_StatusTypeDef ClawLimitStorage_ProgramRecord(uint32_t address, const ClawLimitStorageRecordV2_t *record)
{
    const uint16_t *halfwords;
    uint32_t offset;
    uint32_t commit_offset;
    HAL_StatusTypeDef status;

    halfwords = (const uint16_t *)record;
    commit_offset = (uint32_t)offsetof(ClawLimitStorageRecordV2_t, commit);
    status = HAL_OK;
    for (offset = 0UL; offset < commit_offset; offset += 2UL) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address + offset, halfwords[offset / 2UL]);
        if (status != HAL_OK) {
            return status;
        }
    }
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                               address + commit_offset,
                               (uint16_t)(record->commit & 0xFFFFUL));
    if (status != HAL_OK) {
        return status;
    }
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                             address + commit_offset + 2UL,
                             (uint16_t)((record->commit >> 16) & 0xFFFFUL));
}