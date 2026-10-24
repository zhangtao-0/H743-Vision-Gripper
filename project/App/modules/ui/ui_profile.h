#ifndef UI_PROFILE_H
#define UI_PROFILE_H
#include <stdint.h>
#define UI_PROFILE_MAGIC 0x55495031UL
#define UI_PROFILE_VERSION 1U
#define UI_PROFILE_VIEW_TAG 0x56494557UL /* reserved[0]; v1 extension, zero means legacy */
/* 160 bytes = five H743 256-bit Flashwords. flags=0 is an atomic tombstone. */
typedef struct {
 uint32_t magic,version,sequence,flags;
 int32_t sts[21];
 uint32_t motor_target,kp_milli,ki_milli,kd_milli,cpr_bits;
 uint32_t reserved[9],crc;
} UiProfile;
uint32_t UiProfile_Crc(const UiProfile *);
uint8_t UiProfile_Valid(const UiProfile *);
int UiProfile_Latest(const UiProfile *,const UiProfile *);
#endif
