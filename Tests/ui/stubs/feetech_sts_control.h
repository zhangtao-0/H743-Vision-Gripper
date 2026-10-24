#include <stdint.h>
typedef int FeetechStsParamId_t;
typedef struct {int32_t value,min_value,max_value,default_value;} FeetechStsParamInfo_t;
uint8_t FeetechStsControl_GetParamInfo(FeetechStsParamId_t,FeetechStsParamInfo_t*);
uint8_t FeetechStsControl_GetParamValue(FeetechStsParamId_t,int32_t*);
uint8_t FeetechStsControl_SetParamValue(FeetechStsParamId_t,int32_t);
uint8_t FeetechStsControl_IsMotionState(void);
