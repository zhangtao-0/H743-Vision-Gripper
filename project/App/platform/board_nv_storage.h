#ifndef BOARD_NV_STORAGE_H
#define BOARD_NV_STORAGE_H
/* HAL-free, region-scoped NV interface. Backend owns erase/program/cache rules. */
#include <stdint.h>
typedef enum {BOARD_NV_PROFILE_A,BOARD_NV_PROFILE_B,BOARD_NV_LIMIT_A,BOARD_NV_LIMIT_B,BOARD_NV_COUNT} BoardNvRegion;
int BoardNv_Read(BoardNvRegion region,void *out,uint32_t bytes);
/* Rewrite one reserved region. Caller must select inactive slot and retain the old valid slot. */
int BoardNv_Rewrite(BoardNvRegion region,const void *data,uint32_t bytes);
#endif
