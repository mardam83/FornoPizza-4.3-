/**
 * hardware.h — Forno Pizza Controller
 *             GUITION JC4827W543C — ESP32-S3-WROOM-1-N4R8
 *
 * ================================================================
 *  HARDWARE:
 *    Display : NV3041A  QSPI  480×272
 *    Touch   : GT911    I2C
 *    Sensori : MAX6675 × 2  SPI software
 *    Relè    : 4× (Base, Cielo, Luce, Fan)
 *    PSRAM   : 8MB OPI  → GPIO33-37 RISERVATI internamente
 *    Flash   : 4MB      → PartitionScheme: default
 *
 *  ⚠ GPIO VIETATI su N4R8 (8MB PSRAM OPI):
 *    GPIO26-32  : SPI0/1 flash interno
 *    GPIO33-37  : PSRAM OPI 8-line  ← VIETATI su N4R8!
 *    GPIO19-20  : USB D-/D+
 *    GPIO43-44  : UART0 console
 *
 *  GPIO usati dal display/touch (NON usare per altro):
 *    GPIO47          : QSPI CLK
 *    GPIO21,48,40,39 : QSPI D0-D3
 *    GPIO45          : QSPI CS
 *    GPIO1           : Backlight PWM
 *    GPIO8           : Touch SDA
 *    GPIO4           : Touch SCL
 *    GPIO3           : Touch INT
 *    GPIO38          : Touch RST
 * ================================================================
 */

#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ================================================================
//  DISPLAY — NV3041A QSPI
// ================================================================
#define LCD_CLK   47
#define LCD_D0    21
#define LCD_D1    48
#define LCD_D2    40
#define LCD_D3    39
#define LCD_CS    45
#define LCD_BL     1

#define LCD_H_RES  480
#define LCD_V_RES  272
#define DISP_W     LCD_H_RES
#define DISP_H     LCD_V_RES

// ================================================================
//  TOUCH — GT911  I2C
// ================================================================
#define TOUCH_SDA        8
#define TOUCH_SCL        4
#define TOUCH_INT        3
#define TOUCH_RST       38
#define TOUCH_I2C_ADDR  0x5D   // alternativo: 0x5D

// ================================================================
//  MAX6675 — SPI SOFTWARE (GPIO liberi su N4R8)
// ================================================================
#define TC_SCK       12
#define TC_MISO      13
#define TC_CS_BASE   14
#define TC_CS_CIELO  15

// ================================================================
//  RELÈ — GPIO liberi su N4R8: 10, 11, 17, 18
// ================================================================
#define RELAY_BASE   10
#define RELAY_CIELO  11
#define RELAY_LUCE   17
#define RELAY_FAN    18

#define RELAY_BASE_INV   false
#define RELAY_CIELO_INV  false
#define RELAY_LUCE_INV   false
#define RELAY_FAN_INV    false

// ================================================================
//  MACRO RELÈ
//  RELAY_WRITE(pin, inv, state) — usato in .ino / autotune / watchdog
//  RELAY_ON / RELAY_OFF — usati in ui_events.cpp
//  RELAY_INIT — usato in setup()
// ================================================================
#define RELAY_INIT(pin, inv) \
  do { pinMode((pin), OUTPUT); \
       digitalWrite((pin), (inv) ? HIGH : LOW); } while(0)

#define RELAY_ON(pin, inv)  digitalWrite((pin), (inv) ? LOW  : HIGH)
#define RELAY_OFF(pin, inv) digitalWrite((pin), (inv) ? HIGH : LOW)

#define RELAY_WRITE(pin, inv, state) \
  digitalWrite((pin), ((state) ^ (inv)) ? HIGH : LOW)

// ================================================================
//  MUTEX CONDIVISO (definito in FornoPizza_S3.ino)
// ================================================================
extern SemaphoreHandle_t g_mutex;

#define MUTEX_TIMEOUT_MS  50

// MUTEX_TAKE() — timeout fisso MUTEX_TIMEOUT_MS, usato in .ino
#define MUTEX_TAKE() \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE)

// MUTEX_TAKE_MS(ms) — timeout variabile, usato in autotune/wifi/ui_events
#define MUTEX_TAKE_MS(ms) \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(ms)) == pdTRUE)

#define MUTEX_GIVE() xSemaphoreGive(g_mutex)

// ================================================================
//  TASK CONFIGURATION
//  ⚠ I nomi _PRIO (non _PRIORITY) sono quelli usati in .ino
// ================================================================
#define TASK_LVGL_CORE   0
#define TASK_LVGL_PRIO   1
#define TASK_LVGL_STACK  16384   // FIX: 8192→16384, overflow con UI 480×272

#define TASK_WIFI_CORE   0
#define TASK_WIFI_PRIO   1
#define TASK_WIFI_STACK  8192

#define TASK_PID_CORE    1
#define TASK_PID_PRIO    2
#define TASK_PID_STACK   4096

#define TASK_WDG_CORE    1
#define TASK_WDG_PRIO    3
#define TASK_WDG_STACK   2048

// ================================================================
//  PARAMETRI RELAY PID
//  Usati in pid_ctrl.h
// ================================================================
#define RELAY_DUTY_MIN_PCT    5
#define RELAY_DUTY_MAX_PCT   95
#define PREHEAT_MARGIN_DEG   10.0f

// ================================================================
//  PARAMETRI SICUREZZA
//  Usati in FornoPizza_S3.ino
// ================================================================
#define TEMP_MAX_SAFE        480.0f
#define FAN_OFF_TEMP          80.0f
#define WDG_TIMEOUT_MS        5000
#define WDG_CHECK_MS           500
#define TC_READ_TIMEOUT_MS    2000

#define RUNAWAY_DOWN_ENABLED   1
#define RUNAWAY_DOWN_MS        300000
#define RUNAWAY_MIN_DROP       60.0f
#define RUNAWAY_RECOVERY_DEG   15.0f
#define RUNAWAY_UP_ENABLED     1
#define RUNAWAY_RISE_DEG       40.0f
#define RUNAWAY_RISE_MS        8000
