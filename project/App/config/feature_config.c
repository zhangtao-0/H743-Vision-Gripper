/*********************************************************************************************************************
 * @file    feature_config.c
 * @brief   夹爪默认运行配置实例。
 *********************************************************************************************************************/

#include "feature_config.h"

// ======================================================
// 1. 默认运行配置
// ======================================================

const ClawFeatureDefaults_t g_claw_feature_defaults = {
    CLAW_DEFAULT_AUTO_PRINT_ENABLE,
    CLAW_DEFAULT_AUTO_PRINT_PERIOD_MS,
    CLAW_DEFAULT_MOTION_RANGE,
    CLAW_DEFAULT_RANGE_SELECT_HOLD_MS,
    CLAW_DEFAULT_LIMIT_COMBO_HOLD_MS,
    CLAW_DEFAULT_CALIBRATION_SWEEP_SPEED,
    CLAW_DEFAULT_STALL_STARTUP_MS,
    CLAW_DEFAULT_STALL_TIMEOUT_MS,
    CLAW_DEFAULT_STALL_MIN_DELTA_POSITION,
    CLAW_DEFAULT_STALL_TARGET_MARGIN_POSITION
};
