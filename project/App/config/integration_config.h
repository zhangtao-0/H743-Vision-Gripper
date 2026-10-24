#ifndef INTEGRATION_CONFIG_H
#define INTEGRATION_CONFIG_H
#include "target_config.h"
#include APP_BOARD_PROFILE_HEADER
/* All modules use the target project HAL; sensor selection is mutually exclusive. */
#ifndef BOARD_CAMERA_SENSOR
#define BOARD_CAMERA_SENSOR 5640
#endif
#if BOARD_CAMERA_SENSOR != 5640 && BOARD_CAMERA_SENSOR != 2640
#error "Unsupported camera sensor"
#endif
#ifndef BOARD_ENABLE_LCD
#define BOARD_ENABLE_LCD 1
#endif
#ifndef BOARD_ENABLE_CAMERA
#define BOARD_ENABLE_CAMERA 1
#endif
#ifndef BOARD_ENABLE_RTC
#define BOARD_ENABLE_RTC 1
#endif
#ifndef BOARD_ENABLE_SD
#define BOARD_ENABLE_SD 1
#endif
#ifndef BOARD_ENABLE_QSPI
#define BOARD_ENABLE_QSPI 1
#endif
#ifndef BOARD_ENABLE_USB_CDC
#define BOARD_ENABLE_USB_CDC 1
#endif
#ifndef BOARD_ENABLE_BLUETOOTH
#define BOARD_ENABLE_BLUETOOTH 1
#endif
#ifndef BOARD_ENABLE_PWM
#define BOARD_ENABLE_PWM 1 /* TIM4 CH1/2, PD12/PD13; PA6 stays with DCMI */
#endif
#ifndef BOARD_ENABLE_UI
#define BOARD_ENABLE_UI 1 /* KEY3/4 menu; capture separated from preview */
#endif
#if BOARD_ENABLE_UI && !BOARD_ENABLE_LCD
#error "UI requires LCD"
#endif
#ifndef BOARD_KEYS34_CONTROL_PWM
#define BOARD_KEYS34_CONTROL_PWM 0 /* 0: preview/status; 1: original PWM buttons */
#endif
#if BOARD_ENABLE_UI && BOARD_KEYS34_CONTROL_PWM
#error "KEY3/4 cannot control UI and PWM together"
#endif
#ifndef BOARD_CAMERA_PCLK_DIV
#define BOARD_CAMERA_PCLK_DIV 4U /* tested on FD5640: 160 MHz / 4 = 40 MHz */
#endif
#if BOARD_CAMERA_PCLK_DIV != 1 && BOARD_CAMERA_PCLK_DIV != 2 && BOARD_CAMERA_PCLK_DIV != 4 && BOARD_CAMERA_PCLK_DIV != 8 && BOARD_CAMERA_PCLK_DIV != 16
#error "Invalid OV5640 manual PCLK divider"
#endif
#define BOARD_CAMERA_WIDTH 240U
#define BOARD_CAMERA_HEIGHT 240U
#define BOARD_CAMERA_STRIP_ROWS 4U
#define BOARD_CAMERA_FRAME_TIMEOUT_MS 500U
/* Explicit capability failures prevent carrying H7-only features onto smaller MCUs. */
#if BOARD_ENABLE_CAMERA && !BOARD_HAS_DCMI
#error "Selected board has no DCMI camera backend; disable camera or provide another transport."
#endif
#if BOARD_ENABLE_USB_CDC && !BOARD_HAS_USB_CDC
#error "Selected board has no USB CDC backend"
#endif
#if BOARD_ENABLE_SD && !BOARD_HAS_SDMMC
#error "Selected board has no SDMMC backend"
#endif
#if BOARD_ENABLE_QSPI && !BOARD_HAS_QSPI
#error "Selected board has no QSPI backend"
#endif
#endif
