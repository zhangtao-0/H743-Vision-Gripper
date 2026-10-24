#include "board_storage.h"
#include "integration_config.h"
#include "board_peripherals.h"
#include "sdmmc_sd.h"
#include "qspi_w25q64.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>

extern QSPI_HandleTypeDef hqspi;
static BoardStorageStatus s_status;
static FATFS s_fs;
static uint32_t s_sector[128]; /* blocking HAL SD path; not used with DMA */

static HAL_StatusTypeDef QspiQuadEnable(void)
{
    QSPI_CommandTypeDef cmd = {0}; uint8_t sr;
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE; cmd.Instruction = 0x35;
    cmd.AddressMode = QSPI_ADDRESS_NONE; cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode = QSPI_DATA_1_LINE; cmd.NbData = 1; cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY; cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    if (HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK || HAL_QSPI_Receive(&hqspi, &sr, 100) != HAL_OK) return HAL_ERROR;
    if (!(sr & 2U)) {
        /* W25Q64JV: volatile status-register write-enable, no EEPROM change. */
        cmd.Instruction = 0x50; cmd.DataMode = QSPI_DATA_NONE;
        if (HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK) return HAL_ERROR;
        cmd.Instruction = 0x31; cmd.DataMode = QSPI_DATA_1_LINE; sr |= 2U;
        if (HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK || HAL_QSPI_Transmit(&hqspi, &sr, 100) != HAL_OK) return HAL_ERROR;
        HAL_Delay(1);
    }
    cmd.Instruction = 0x35; cmd.DataMode = QSPI_DATA_1_LINE;
    if (HAL_QSPI_Command(&hqspi, &cmd, 100) != HAL_OK || HAL_QSPI_Receive(&hqspi, &sr, 100) != HAL_OK || !(sr & 2U)) return HAL_ERROR;
    return HAL_OK;
}

void BoardStorage_Init(void)
{
#if BOARD_ENABLE_QSPI
    s_status.qspi_ready = (QSPI_W25Qxx_Init() == 0);
    if (s_status.qspi_ready) {
        s_status.qspi_id = QSPI_W25Qxx_ReadID();
        if (QspiQuadEnable() != HAL_OK) s_status.qspi_ready = 0;
    }
#endif
#if BOARD_ENABLE_SD
    (void)BoardStorage_Remount();
#endif
}

HAL_StatusTypeDef BoardStorage_Remount(void)
{
#if BOARD_ENABLE_SD
    BSP_SD_CardInfo info;
    (void)f_mount(0, "0:", 0);
    if (s_status.sd_ready) (void)BSP_SD_DeInit(0);
    s_status.fs_ready = 0; s_status.sd_blocks = 0;
    s_status.sd_ready = (BSP_SD_Init(0) == BSP_ERROR_NONE);
    if (s_status.sd_ready && BSP_SD_GetCardInfo(0, &info) == 0) s_status.sd_blocks = info.LogBlockNbr;
    if (s_status.sd_ready) s_status.fs_ready = (f_mount(&s_fs, "0:", 1) == FR_OK);
    return s_status.fs_ready ? HAL_OK : HAL_ERROR;
#else
    return HAL_ERROR;
#endif
}

const BoardStorageStatus *BoardStorage_GetStatus(void) { return &s_status; }

HAL_StatusTypeDef BoardStorage_SdReadTest(void)
{
    if (!s_status.sd_ready) return HAL_ERROR;
    return BSP_SD_ReadBlocks(0, (uint32_t *)s_sector, 0, 1) == 0 ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef BoardStorage_FileTest(void)
{
    FIL file; UINT n; FRESULT result;
    static const char text[] = "H743 integrated FatFs test\r\n";
    char readback[sizeof(text)] = {0};
    if (!s_status.fs_ready) return HAL_ERROR;
    /* Own test file only; existing file is not overwritten. */
    result = f_open(&file, "0:/H743TEST.TXT", FA_WRITE | FA_CREATE_NEW);
    if (result == FR_EXIST) return HAL_BUSY;
    if (result != FR_OK) return HAL_ERROR;
    result = f_write(&file, text, sizeof(text) - 1U, &n);
    if (result == FR_OK && n != sizeof(text) - 1U) result = FR_DISK_ERR;
    if (f_sync(&file) != FR_OK) result = FR_DISK_ERR;
    if (f_close(&file) != FR_OK) result = FR_DISK_ERR;
    if (result != FR_OK || f_open(&file, "0:/H743TEST.TXT", FA_READ) != FR_OK) return HAL_ERROR;
    result = f_read(&file, readback, sizeof(text) - 1U, &n);
    (void)f_close(&file);
    return result == FR_OK && n == sizeof(text) - 1U && !memcmp(text, readback, n) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef BoardStorage_QspiMap(uint8_t enable)
{
    if (!s_status.qspi_ready) return HAL_ERROR;
    if (enable && !s_status.qspi_mapped) {
        uint8_t before[16];
        if (QSPI_W25Qxx_ReadBuffer(before, 0, sizeof(before)) != 0) return HAL_ERROR;
        if (QSPI_W25Qxx_MemoryMappedMode() != 0) return HAL_ERROR;
        if (memcmp((const void *)0x90000000, before, sizeof(before))) {
            (void)HAL_QSPI_Abort(&hqspi); return HAL_ERROR;
        }
        s_status.qspi_mapped = 1;
    } else if (!enable && s_status.qspi_mapped) {
        if (HAL_QSPI_Abort(&hqspi) != HAL_OK) return HAL_ERROR;
        s_status.qspi_mapped = 0;
    }
    return HAL_OK;
}

HAL_StatusTypeDef BoardStorage_QspiTest(void)
{
    uint8_t pattern[32], readback[32]; uint32_t i;
    if (!s_status.qspi_ready || BoardStorage_QspiMap(0) != HAL_OK) return HAL_ERROR;
    for (i = 0; i < sizeof(pattern); i++) pattern[i] = (uint8_t)(0xA5U ^ i);
    /* Explicit command only; test owns last 4KiB sector of external W25Q64. */
    if (QSPI_W25Qxx_SectorErase(BOARD_QSPI_TEST_SECTOR) != 0 ||
        QSPI_W25Qxx_WriteBuffer(pattern, BOARD_QSPI_TEST_SECTOR, sizeof(pattern)) != 0 ||
        QSPI_W25Qxx_ReadBuffer(readback, BOARD_QSPI_TEST_SECTOR, sizeof(readback)) != 0) return HAL_ERROR;
    return !memcmp(pattern, readback, sizeof(pattern)) ? HAL_OK : HAL_ERROR;
}

/* FatFs disk port replaces the sample's unchecked driver table. */
DSTATUS disk_status(BYTE drive) { return drive == 0 && s_status.sd_ready ? 0 : STA_NOINIT; }
DSTATUS disk_initialize(BYTE drive) { return disk_status(drive); }
DRESULT disk_read(BYTE drive, BYTE *buf, DWORD sector, UINT count)
{
    if (drive || !buf || !count || sector >= s_status.sd_blocks || count > s_status.sd_blocks - sector) return RES_PARERR;
    if (!s_status.sd_ready) return RES_NOTRDY;
    {
        UINT i;
        for (i=0;i<count;i++) {
            if (BSP_SD_ReadBlocks(0,s_sector,sector+i,1) != 0) return RES_ERROR;
            memcpy(buf+i*512U,s_sector,512U);
        }
        return RES_OK;
    }
}
DRESULT disk_write(BYTE drive, const BYTE *buf, DWORD sector, UINT count)
{
    if (drive || !buf || !count || sector >= s_status.sd_blocks || count > s_status.sd_blocks - sector) return RES_PARERR;
    if (!s_status.sd_ready) return RES_NOTRDY;
    {
        UINT i; uint32_t start;
        for (i=0;i<count;i++) {
            memcpy(s_sector,buf+i*512U,512U);
            if (BSP_SD_WriteBlocks(0,s_sector,sector+i,1) != 0) return RES_ERROR;
            start=HAL_GetTick();
            while (BSP_SD_GetCardState(0)!=SD_TRANSFER_OK) if (HAL_GetTick()-start>1000U) return RES_ERROR;
        }
        return RES_OK;
    }
}
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buf)
{
    uint32_t start;
    if (drive || !s_status.sd_ready) return RES_NOTRDY;
    if (cmd == CTRL_SYNC) {
        start = HAL_GetTick();
        while (BSP_SD_GetCardState(0) != SD_TRANSFER_OK) if (HAL_GetTick() - start > 1000U) return RES_ERROR;
        return RES_OK;
    }
    if (!buf) return RES_PARERR;
    if (cmd == GET_SECTOR_COUNT) *(DWORD *)buf = s_status.sd_blocks;
    else if (cmd == GET_SECTOR_SIZE) *(WORD *)buf = 512;
    else if (cmd == GET_BLOCK_SIZE) *(DWORD *)buf = 1;
    else return RES_PARERR;
    return RES_OK;
}
DWORD get_fattime(void)
{
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    if (BoardRtc_Get(&t, &d) != HAL_OK) return (DWORD)(2026 - 1980) << 25 | 1U << 21 | 1U << 16;
    return ((DWORD)(d.Year + 20U) << 25) | ((DWORD)d.Month << 21) | ((DWORD)d.Date << 16) |
           ((DWORD)t.Hours << 11) | ((DWORD)t.Minutes << 5) | ((DWORD)t.Seconds >> 1);
}
