/**
 * hardware.h — Forno Pizza Controller — ESP32-S3 + JC4827W543C
 *
 * Display:  JC4827W543C  480×272  ST7701S  SPI 4-wire
 * Touch:    CST820  I2C
 * Sensori:  MAX6675 x2  SPI software
 * Relè:     4x (Base, Cielo, Luce, Fan)
 */

#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ================================================================
//  DISPLAY — ST7701S  SPI 4-wire
// ================================================================
#define LCD_MOSI  6
#define LCD_CLK   7
#define LCD_CS    5
#define LCD_DC    4
#define LCD_RST  48
#define LCD_BL   45

#define LCD_H_RES  480
#define LCD_V_RES  272

// ================================================================
//  TOUCH — CST820  I2C
// ================================================================
#define TOUCH_SDA      8
#define TOUCH_SCL      9
#define TOUCH_INT      3
#define TOUCH_RST     38
#define TOUCH_I2C_ADDR 0x15

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

#define MUTEX_TIMEOUT_MS  10

// MUTEX_TAKE() — usa il timeout di default MUTEX_TIMEOUT_MS
#define MUTEX_TAKE() \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE)

// MUTEX_TAKE_MS(ms) — timeout personalizzato in millisecondi
//   Usato da autotune.cpp, ui_events.cpp, wifi_mqtt.cpp
#define MUTEX_TAKE_MS(ms) \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(ms)) == pdTRUE)

#define MUTEX_GIVE() xSemaphoreGive(g_mutex)

// ================================================================
//  PARAMETRI RELAY PID
// ================================================================
#define RELAY_DUTY_MIN_PCT    5
#define RELAY_DUTY_MAX_PCT   95
#define PREHEAT_MARGIN_DEG   10.0f

// ================================================================
//  PARAMETRI SICUREZZA
// ================================================================
#define TEMP_MAX_SAFE        480.0f
#define FAN_OFF_TEMP          80.0f
#define WDG_TIMEOUT_MS       5000
#define WDG_CHECK_MS          500
#define TC_READ_TIMEOUT_MS   2000

#define RUNAWAY_DOWN_ENABLED   1
#define RUNAWAY_DOWN_MS       300000
#define RUNAWAY_MIN_DROP       60.0f
#define RUNAWAY_RECOVERY_DEG   15.0f

#define RUNAWAY_UP_ENABLED     1
#define RUNAWAY_RISE_DEG       40.0f
#define RUNAWAY_RISE_MS        8000
