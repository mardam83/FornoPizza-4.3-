/**
 * hardware.h — Forno Pizza Controller — ESP32-S3 + JC4827W543C
 *
 * Display:  JC4827W543C  480×272  ST7701S  SPI 4-wire
 * Touch:    CST820  I2C
 * Sensori:  MAX6675 x2  SPI software
 * Relè:     3x meccanici (Base, Cielo, Luce)
 *
 * ================================================================
 *  PINOUT JC4827W543C (scheda con connettore FPC integrato)
 * ================================================================
 *  Il modulo JC4827W543C espone pin fissi sul PCB:
 *
 *  LCD ST7701S (SPI 4-wire):
 *    MOSI  = GPIO6
 *    CLK   = GPIO7
 *    CS    = GPIO5
 *    DC    = GPIO4
 *    RST   = GPIO48
 *    BL    = GPIO45   (PWM backlight, HIGH=ON)
 *
 *  Touch CST820 (I2C):
 *    SDA   = GPIO8
 *    SCL   = GPIO9
 *    INT   = GPIO3
 *    RST   = GPIO38
 *    Addr  = 0x15
 *
 *  Note pin:
 *    GPIO19/20 → USB D-/D+ — evitare
 *    GPIO26-GPIO32 → SPI0/1 flash — VIETATI
 *    GPIO33-GPIO37 → PSRAM — VIETATI su N4R2
 *    GPIO0  → Strapping BOOT — evitare come output
 *    GPIO45 → Strapping VDD_SPI — su N4R2 libero dopo boot (usato BL)
 *    GPIO43/44 → UART0 TX/RX — evitare
 *
 * ================================================================
 *  PIN liberi per MAX6675 e RELÈ
 * ================================================================
 *  SPI software MAX6675:
 *    SCK   = GPIO11
 *    MISO  = GPIO12
 *    CS_BASE  = GPIO13
 *    CS_CIELO = GPIO14
 *
 *  Relè:
 *    BASE  = GPIO15
 *    CIELO = GPIO16
 *    LUCE  = GPIO17
 *    FAN   = GPIO10
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
#define LCD_BL   45    // Backlight PWM (LEDC ch0)

// Risoluzione
#define LCD_H_RES  480
#define LCD_V_RES  272

// ================================================================
//  TOUCH — CST820  I2C
// ================================================================
#define TOUCH_SDA    8
#define TOUCH_SCL    9
#define TOUCH_INT    3
#define TOUCH_RST   38
#define TOUCH_I2C_ADDR  0x15

// ================================================================
//  SENSORI TEMPERATURA — MAX6675  SPI software
//  GPIO 11-14: sicuri, nessun conflitto con flash/PSRAM su N4R2
// ================================================================
#define TC_SCK       11
#define TC_MISO      12
#define TC_CS_BASE   13
#define TC_CS_CIELO  14

// ================================================================
//  RELÈ — logica diretta (HIGH = ON)
//  GPIO 15-17: sicuri su N4R2
// ================================================================
#define RELAY_BASE   15
#define RELAY_CIELO  16
#define RELAY_LUCE   17
#define RELAY_FAN    10   // Quarto relè — ventola di raffreddamento

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
#define MUTEX_TAKE()      (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE)
#define MUTEX_GIVE()       xSemaphoreGive(g_mutex)

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
