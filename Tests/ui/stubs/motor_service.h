#include <stdint.h>
typedef struct {struct {struct {float max_rpm,kp,ki,kd,counts_per_rev;} config;float target_rpm;} control;} MotorStatus;
void MotorService_Snapshot(MotorStatus*);
uint8_t MotorService_ApplyParameters(float,uint32_t,uint32_t,uint32_t,uint32_t);
uint8_t MotorService_BeginMaintenance(void);
void MotorService_EndMaintenance(void);
