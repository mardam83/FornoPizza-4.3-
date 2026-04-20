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
//
//  L'indirizzo I2C del GT911 dipende dal livello su INT durante reset:
//    INT=LOW  durante reset  →  0x5D  (sequenza usata in gt911_init())
//    INT=HIGH durante reset  →  0x14
//
//  La sequenza reset in display_driver.h forza INT=LOW → addr 0x5D.
//  Se lo scan I2C trova 0x14 invece di 0x5D, invertire la logica
//  del pin INT nella sequenza gt911_init() in display_driver.h.
// ================================================================
#define TOUCH_SDA        8
#define TOUCH_SCL        4
#define TOUCH_INT        3
#define TOUCH_RST       38
#define TOUCH_I2C_ADDR  0x5D   // 0x5D con INT=LOW al reset; alternativo: 0x14

// ================================================================
//  MAX6675 — SPI SOFTWARE
//  Connettore P2: IO46=MISO  IO9=SCK  IO14=CS_BASE  IO5=CS_CIELO
//  ⚠ IO46 = strapping ROM print (pull-down) → safe come MISO input
// ================================================================
#define TC_SCK       9    // P2 — era 12
#define TC_MISO      46   // P2 — era 13 (strapping, safe come input)
#define TC_CS_BASE   14   // P2 — invariato
#define TC_CS_CIELO  5    // P2 — era 15

// ================================================================
//  RELÈ — GPIO liberi su N4R8
//  Connettore P3: IO6=BASE  IO7=CIELO  IO15=LUCE  IO16=FAN
//  (P4 IO17/IO18 tenuti liberi come riserva)
// ================================================================
#define RELAY_BASE   18    // P3 — era 10
#define RELAY_CIELO  17    // P3 — era 11
#define RELAY_LUCE   15   // P3 — era 17
#define RELAY_FAN    16   // P3 — era 18

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

#define MUTEX_TAKE() \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE)

#define MUTEX_TAKE_MS(ms) \
  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(ms)) == pdTRUE)

#define MUTEX_GIVE() xSemaphoreGive(g_mutex)

// ================================================================
//  TASK CONFIGURATION
// ================================================================
#define TASK_LVGL_CORE   0
#define TASK_LVGL_PRIO   1
#define TASK_LVGL_STACK  16384

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
// ================================================================
#define RELAY_DUTY_MIN_PCT    5
#define RELAY_DUTY_MAX_PCT   95
#define PREHEAT_MARGIN_DEG   10.0f

// ================================================================
//  PARAMETRI SICUREZZA
// ================================================================
#define TEMP_MAX_SAFE        480.0f
#define FAN_OFF_TEMP          80.0f
#define WDG_TIMEOUT_MS        5000
#define WDG_CHECK_MS           500
#define TC_READ_TIMEOUT_MS    2000

// Runaway — riferimento: camera ~35×35×10 cm, resistenze totali ~2200 W (vedi modello in simulator.h).
// RUNAWAY_UP: salita rapida con relay logicamente spenti (es. SSR incollato / perdita controllo).
// RUNAWAY_DOWN: dopo essere stati vicini al setpoint, calo forte con richiesta calore ancora attiva
//               (elemento aperto, sensore disancorato, ecc.).
#define RUNAWAY_DOWN_ENABLED   1
#define RUNAWAY_DOWN_MS        300000
#define RUNAWAY_MIN_DROP       60.0f
#define RUNAWAY_RECOVERY_DEG   15.0f
#define RUNAWAY_UP_ENABLED     1
#define RUNAWAY_RISE_DEG       40.0f
#define RUNAWAY_RISE_MS        8000
