/**
 * hardware.h — Forno Pizza Controller — ESP32-S3 + JC4827W543C
 *
 * Display:  JC4827W543C  480×272  ST7701A  SPI 4-wire
 * Touch:    CST820  I2C
 * Sensori:  MAX6675 x2  SPI software
 * Relè:     4x (Base, Cielo, Luce, Fan)
 *
 * FIX v2:
 *   - TASK_LVGL_STACK: 8192 → 16384  (overflow con UI 480×272 + font)
 *   - MUTEX_TIMEOUT_MS: 10 → 50      (Task_PID perdeva il mutex)
 *   - TASK config documentata
 */

#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ================================================================
//  DISPLAY — ST7701A  SPI 4-wire  (usato con DISPLAY_MODE_RGB=0)
// ================================================================
#define LCD_MOSI  6
#define LCD_CLK   7
#define LCD_CS    5
#define LCD_DC    4
#define LCD_RST  48
#define LCD_BL   45

// ================================================================
//  DISPLAY — RGB parallelo 16-bit  (usato con DISPLAY_MODE_RGB=1)
//  Pinout JC4827W543C da schematica ufficiale
// ================================================================
#define LCD_RGB_R0   45   // R0
#define LCD_RGB_R1   48   // R1
#define LCD_RGB_R2   47   // R2
#define LCD_RGB_R3   21   // R3
#define LCD_RGB_R4   14   // R4
#define LCD_RGB_G0    5   // G0
#define LCD_RGB_G1    6   // G1
#define LCD_RGB_G2    7   // G2
#define LCD_RGB_G3   15   // G3
#define LCD_RGB_G4   16   // G4
#define LCD_RGB_G5    4   // G5
#define LCD_RGB_B0    8   // B0
#define LCD_RGB_B1    3   // B1
#define LCD_RGB_B2   46   // B2
#define LCD_RGB_B3    9   // B3
#define LCD_RGB_B4    1   // B4
#define LCD_RGB_HSYNC  39
#define LCD_RGB_VSYNC  41
#define LCD_RGB_DE     40
#define LCD_RGB_PCLK   42
// BL è comune ad entrambe le modalità: GPIO45 (SPI) o GPIO2 (RGB)
// Se RGB: cambia LCD_BL a 2 in debug_config.h o qui sotto
// #undef  LCD_BL
// #define LCD_BL  2

#define LCD_H_RES  480
#define LCD_V_RES  272

// ================================================================
//  TOUCH — CST820  I2C
// ================================================================
#define TOUCH_SDA       8
#define TOUCH_SCL       9
#define TOUCH_INT       3
#define TOUCH_RST      38
#define TOUCH_I2C_ADDR  0x15

// ================================================================
//  SENSORI TEMPERATURA — MAX6675  SPI software
// ================================================================
#define TC_SCK       11
#define TC_MISO      12
#define TC_CS_BASE   13
#define TC_CS_CIELO  14

// ================================================================
//  RELÈ — logica diretta (HIGH = ON)
// ================================================================
#define RELAY_BASE   15
#define RELAY_CIELO  16
#define RELAY_LUCE   17
#define RELAY_FAN    10

#define RELAY_BASE_INV   false
#define RELAY_CIELO_INV  false
#define RELAY_LUCE_INV   false
#define RELAY_FAN_INV    false

// ================================================================
//  MACRO RELAY
// ================================================================
#define RELAY_INIT(pin, inv) \
  do { pinMode((pin), OUTPUT); \
       digitalWrite((pin), (inv) ? HIGH : LOW); } while(0)

#define RELAY_WRITE(pin, inv, state) \
  digitalWrite((pin), ((state) ^ (inv)) ? HIGH : LOW)

// ================================================================
//  MUTEX CONDIVISO
// ================================================================
extern SemaphoreHandle_t g_mutex;

// FIX: 10ms → 50ms  (LVGL tiene il mutex ~20ms durante ui_refresh())
#define MUTEX_TIMEOUT_MS  50

#define MUTEX_TAKE() \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE)

#define MUTEX_TAKE_MS(ms) \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(ms)) == pdTRUE)

#define MUTEX_GIVE() xSemaphoreGive(g_mutex)

// ================================================================
//  TASK CONFIG
//  Core 0: LVGL + WiFi  (stesso core dello stack WiFi ESP-IDF)
//  Core 1: PID + Watchdog  (real-time, nessuna interruzione da WiFi)
// ================================================================

// FIX: stack 8192 → 16384 (overflow con LVGL 8.x + font + schermate multiple)
#define TASK_LVGL_CORE   0
#define TASK_LVGL_PRIO   1
#define TASK_LVGL_STACK  16384

// WiFi in RAM interna — stack normale, non spostare in PSRAM
// (lo stack WiFi/MQTT usa operazioni atomiche e ISR che richiedono DRAM)
#define TASK_WIFI_CORE   0
#define TASK_WIFI_PRIO   1
#define TASK_WIFI_STACK  8192

// PID in RAM interna — real-time, nessuna latenza PSRAM
#define TASK_PID_CORE    1
#define TASK_PID_PRIO    2
#define TASK_PID_STACK   4096

// Watchdog in RAM interna — deve rispondere anche sotto pressione
#define TASK_WDG_CORE    1
#define TASK_WDG_PRIO    3
#define TASK_WDG_STACK   2048

// ================================================================
//  DISPLAY RESOLUTION (alias per compatibilità)
// ================================================================
#define DISP_W  LCD_H_RES
#define DISP_H  LCD_V_RES

// ================================================================
//  PARAMETRI RELAY PID
// ================================================================
#define RELAY_DUTY_MIN_PCT    5
#define RELAY_DUTY_MAX_PCT   95
#define PREHEAT_MARGIN_DEG   10.0f

// ================================================================
//  PARAMETRI SICUREZZA
// ================================================================
#define TEMP_MAX_SAFE         480.0f
#define FAN_OFF_TEMP           80.0f
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
