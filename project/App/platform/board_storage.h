#ifndef BOARD_STORAGE_H
#define BOARD_STORAGE_H
#include "platform_hal.h"
typedef struct { uint8_t sd_ready, fs_ready, qspi_ready, qspi_mapped; uint32_t qspi_id, sd_blocks; } BoardStorageStatus;
void BoardStorage_Init(void);
HAL_StatusTypeDef BoardStorage_Remount(void);
HAL_StatusTypeDef BoardStorage_FileTest(void);
HAL_StatusTypeDef BoardStorage_SdReadTest(void);
HAL_StatusTypeDef BoardStorage_QspiTest(void);
HAL_StatusTypeDef BoardStorage_QspiMap(uint8_t enable);
const BoardStorageStatus *BoardStorage_GetStatus(void);
#endif
