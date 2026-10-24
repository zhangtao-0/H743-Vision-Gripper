/*********************************************************************************************************************
 * @file    shared_state.c
 * @brief   夹爪公共运行状态和执行变量实例。
 *********************************************************************************************************************/

#include "shared_state.h"
#include <string.h>

// ======================================================
// 1. 公共控制状态
// ======================================================

FeetechStsControlStatus_t g_claw_control_state;

// ======================================================
// 2. 公共配置和运行快照
// ======================================================

FeetechStsManager_t g_claw_manager_state;

// ======================================================
// 3. 真实总线数据入口
// ======================================================

const FeetechStsFeedback_t *ClawSharedState_GetFeedback(void)
{
    return FeetechStsBus_GetFeedback();
}

const FeetechStsMotion_t *ClawSharedState_GetMotion(void)
{
    return FeetechStsBus_GetMotion();
}

// ======================================================
// 4. 生命周期
// ======================================================

void ClawSharedState_Reset(void)
{
    memset(&g_claw_control_state, 0, sizeof(g_claw_control_state));
    memset(&g_claw_manager_state, 0, sizeof(g_claw_manager_state));
}
