/**
 * display_driver.h — Arduino_GFX (NV3041A QSPI) + LVGL + GT911 (Wire I2C)
 *
 * ================================================================
 *  HARDWARE: GUITION JC4827W543C
 *    Display : NV3041A — QSPI (Quad SPI)  480×272
 *    Touch   : GT911   — I2C capacitivo
 *    MCU     : ESP32-S3-WROOM-1-N4R8
 *
 *  PERCHÉ Arduino_GFX invece di LovyanGFX:
 *    LovyanGFX non ha Bus_QSPI stabile su arduino-esp32 >= 3.x.
 *    Arduino_GFX (moononournation) ha Arduino_ESP32QSPI + Arduino_NV3041A
 *    verificati e funzionanti su questo hardware.
 *
 *  LIBRERIE RICHIESTE (Library Manager):
 *    - GFX Library for Arduino  (by moononournation)  >= 1.4.6
 *    - lvgl                     >= 8.3.0
 *    (NON serve LovyanGFX per questo display)
 *
 *  PINOUT (da pinout ufficiale GUITION/ESPHome):
 *    QSPI CS   : GPIO45
 *    QSPI CLK  : GPIO47
 *    QSPI D0   : GPIO21
 *    QSPI D1   : GPIO48
 *    QSPI D2   : GPIO40
 *    QSPI D3   : GPIO39
 *    Backlight : GPIO1
 *    Touch SDA : GPIO8
 *    Touch SCL : GPIO4
 *    Touch INT : GPIO3
 *    Touch RST : GPIO38
 * ================================================================
 */

#pragma once

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "hardware.h"
#include "debug_config.h"


#ifndef BLACK
#define BLACK 0x0000
#endif
// ================================================================
//  ARDUINO_GFX — Bus QSPI + Panel NV3041A
// ================================================================
static Arduino_DataBus* s_bus = nullptr;
static Arduino_GFX*     s_gfx = nullptr;

// ================================================================
//  TOUCH GT911 — lettura diretta via Wire (nessuna libreria extra)
//  Protocollo I2C GT911: read 6 byte da 0x814E
// ================================================================
#define GT911_ADDR      TOUCH_I2C_ADDR   // 0x14 (o 0x5D)
#define GT911_REG_STAT  0x814E
#define GT911_REG_PT1   0x8150

static bool gt911_read(uint16_t* tx, uint16_t* ty) {
  // Leggi registro status
  Wire.beginTransmission(GT911_ADDR);
  Wire.write((uint8_t)(GT911_REG_STAT >> 8));
  Wire.write((uint8_t)(GT911_REG_STAT & 0xFF));
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)GT911_ADDR, (uint8_t)1);
  if (!Wire.available()) return false;
  uint8_t stat = Wire.read();
  uint8_t npts = stat & 0x0F;

  // Pulisci flag status
  Wire.beginTransmission(GT911_ADDR);
  Wire.write((uint8_t)(GT911_REG_STAT >> 8));
  Wire.write((uint8_t)(GT911_REG_STAT & 0xFF));
  Wire.write(0x00);
  Wire.endTransmission();

  if (!(stat & 0x80) || npts == 0) return false;

  // Leggi coordinate primo punto (4 byte: x_lo, x_hi, y_lo, y_hi + altri)
  Wire.beginTransmission(GT911_ADDR);
  Wire.write((uint8_t)(GT911_REG_PT1 >> 8));
  Wire.write((uint8_t)(GT911_REG_PT1 & 0xFF));
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)GT911_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;

  Wire.read(); // track ID
  uint8_t xl = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t yl = Wire.read();
  uint8_t yh = Wire.read();
  Wire.read(); // size

  *tx = (uint16_t)xl | ((uint16_t)xh << 8);
  *ty = (uint16_t)yl | ((uint16_t)yh << 8);
  return true;
}

static void gt911_init() {
  // Reset GT911
  if (TOUCH_RST >= 0) {
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
  }
  // INT pin come input
  if (TOUCH_INT >= 0) {
    pinMode(TOUCH_INT, INPUT);
  }
  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);
  delay(50);
  Serial.printf("[TOUCH] GT911 inizializzato (addr=0x%02X, SDA=%d, SCL=%d)\n",
    GT911_ADDR, TOUCH_SDA, TOUCH_SCL);
}

// ================================================================
//  BUFFER LVGL
// ================================================================
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t* s_fb1 = nullptr;
static lv_color_t* s_fb2 = nullptr;

// ================================================================
//  CALLBACK FLUSH LVGL → Arduino_GFX
// ================================================================
static void lv_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  s_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)&color_p->full, w, h);
  lv_disp_flush_ready(drv);
}

// ================================================================
//  CALLBACK TOUCH LVGL
// ================================================================
static void lv_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  uint16_t x = 0, y = 0;
  if (gt911_read(&x, &y)) {
    // NV3041A montato a 180° su JC4827W543C → specchia coordinate
    x = LCD_H_RES - 1 - x;
    y = LCD_V_RES - 1 - y;
    // Clamp per sicurezza
    if (x >= LCD_H_RES) x = LCD_H_RES - 1;
    if (y >= LCD_V_RES) y = LCD_V_RES - 1;
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state   = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ================================================================
//  BACKLIGHT PWM (LEDC)
// ================================================================
static void backlight_init(uint8_t brightness) {
  ledcAttach(LCD_BL, 44100, 8);
  ledcWrite(LCD_BL, brightness);
  Serial.printf("[DISP] Backlight GPIO%d brightness=%d\n", LCD_BL, brightness);
}

// ================================================================
//  display_init() — chiamato UNA SOLA VOLTA da setup()
// ================================================================
void display_init() {
  // ── 1. Bus QSPI + Panel NV3041A ──
  s_bus = new Arduino_ESP32QSPI(
    LCD_CS,   // CS   GPIO45
    LCD_CLK,  // CLK  GPIO47
    LCD_D0,   // D0   GPIO21
    LCD_D1,   // D1   GPIO48
    LCD_D2,   // D2   GPIO40
    LCD_D3    // D3   GPIO39
  );
  s_gfx = new Arduino_NV3041A(
    s_bus,
    GFX_NOT_DEFINED, // RST — non connesso
    2,               // rotation: 2 = 180° (corretto per JC4827W543C)
    true             // IPS = true
  );

  if (!s_gfx->begin(20000000UL)) {  // 20 MHz — stabile su questo hardware
    Serial.println("[DISP] ERRORE: begin() fallito! Verifica cablaggio.");
  } else {
    Serial.printf("[DISP] NV3041A OK  %dx%d\n", s_gfx->width(), s_gfx->height());
  }

  s_gfx->fillScreen(BLACK);
  backlight_init(200);

  // ── 2. Touch GT911 ──
  gt911_init();

  // ── 3. Log memoria ──
  Serial.printf("[MEM] SRAM: %u  PSRAM: %uKB\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

  // ── 4. Buffer LVGL (double FB in PSRAM OPI 8MB) ──
  const size_t fb_bytes = (size_t)LCD_H_RES * LCD_V_RES * sizeof(lv_color_t);
  s_fb1 = (lv_color_t*)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  s_fb2 = (lv_color_t*)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!s_fb1 || !s_fb2) {
    Serial.println("[DISP] WARN: PSRAM non disponibile — fallback SRAM 10 righe");
    if (s_fb1) { heap_caps_free(s_fb1); s_fb1 = nullptr; }
    if (s_fb2) { heap_caps_free(s_fb2); s_fb2 = nullptr; }
    static lv_color_t fallback_buf[LCD_H_RES * 10];
    lv_disp_draw_buf_init(&s_draw_buf, fallback_buf, nullptr, LCD_H_RES * 10);
  } else {
    lv_disp_draw_buf_init(&s_draw_buf, s_fb1, s_fb2, (uint32_t)LCD_H_RES * LCD_V_RES);
    Serial.printf("[DISP] Double FB PSRAM: 2×%uKB\n", (unsigned)(fb_bytes / 1024));
  }

  // ── 5. LVGL init ──
  lv_init();

  // ── 6. Display driver LVGL ──
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res      = LCD_H_RES;
  disp_drv.ver_res      = LCD_V_RES;
  disp_drv.flush_cb     = lv_flush_cb;
  disp_drv.draw_buf     = &s_draw_buf;
  disp_drv.full_refresh = (s_fb1 != nullptr) ? 1 : 0;
  lv_disp_drv_register(&disp_drv);

  // ── 7. Touch driver LVGL ──
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touch_cb;
  lv_indev_drv_register(&indev_drv);

  Serial.printf("[MEM] post-init SRAM: %u  PSRAM: %uKB\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  Serial.printf("[DISP] OK %dx%d  full_refresh=%d\n",
    LCD_H_RES, LCD_V_RES, (s_fb1 != nullptr));
}
