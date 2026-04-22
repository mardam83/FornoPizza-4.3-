/**
 * display_driver.h — Arduino_GFX (NV3041A QSPI) + LVGL + GT911 (Wire I2C)
 *                    v24-S3  PARTIAL REFRESH + SRAM BUFFERS
 *
 * ================================================================
 *  HARDWARE: GUITION JC4827W543C
 *    Display : NV3041A — QSPI (Quad SPI)  480×272
 *    Touch   : GT911   — I2C capacitivo
 *    MCU     : ESP32-S3-WROOM-1-N4R8
 *
 * ================================================================
 *  CHANGELOG v24:
 *
 *  [OPT-1] FULL REFRESH → PARTIAL REFRESH
 *          Prima: 2 framebuffer da 480×272 in PSRAM (~520 KB totali).
 *                 Ogni frame ridisegnava l'intera schermata a costo
 *                 ~30 ms (PSRAM read/write lenta + flush QSPI bloccante).
 *          Ora:   2 draw buffer da 480×28 righe in SRAM interna
 *                 (2 × 26.880 B = 53.760 B, ~52 KB).
 *                 LVGL rende solo le aree dirty (partial refresh).
 *                 Budget frame tipico: 3-6 ms render + flush chunk.
 *
 *  [OPT-2] SRAM INTERNA per i buffer di rendering
 *          La SRAM interna è accessibile a cycle rate MCU (~16× PSRAM
 *          random access). I buffer di rendering sono hot-path:
 *          beneficiano del massimo della banda disponibile.
 *
 *  [OPT-3] QSPI clock mantenuto a 20 MHz (richiesta utente).
 *          Il collo di bottiglia è ora il rendering LVGL, non il flush.
 *
 *  [BUG-1] GT911 "sticky press" dopo errore I2C
 *          In presenza di disturbi I2C, gt911_read() tornava false
 *          ma s_last_pressed restava true dal ciclo precedente
 *          (finestra di 20 ms di throttle).
 *          Se l'errore I2C persisteva >1 ciclo (es. EMI da relay),
 *          LVGL vedeva un "tocco infinito" che faceva scattare
 *          click involontari.
 *          FIX: watchdog 200 ms. Dopo 200 ms senza conferma di touch
 *          valido, s_last_pressed è forzato a false anche nel throttle.
 *
 *  [OPT-4] Flag volatile sulla prima lettura per evitare che il
 *          compilatore ottimizzi via s_last_* tra Task_LVGL wake-ups.
 * ================================================================
 *
 *  PINOUT / INDIRIZZO I2C identici alla versione precedente.
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
//  TOUCH GT911 — costanti
// ================================================================
#define GT911_REG_STAT   0x814E
#define GT911_REG_PT1    0x8150
#define GT911_REG_CFG    0x8047
#define GT911_ADDR_0x14  0x14
#define GT911_ADDR_0x5D  0x5D

// BUG-1 FIX: watchdog anti "sticky press" — soglia dopo la quale
// il throttle forza release se nessuna lettura valida è arrivata.
#define GT911_STUCK_WD_MS   200UL

static uint8_t s_gt911_addr = 0x00;

// ----------------------------------------------------------------
//  gt911_i2c_scan / gt911_verify / gt911_init — invariati rispetto
//  alla versione precedente. Solo commenti ripuliti.
// ----------------------------------------------------------------
static void gt911_i2c_scan() {
  Serial.println("[TOUCH] I2C scan...");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("[TOUCH]   I2C dev @ 0x%02X\n", addr);
      found++;
      if (addr == GT911_ADDR_0x14 || addr == GT911_ADDR_0x5D) {
        s_gt911_addr = addr;
        Serial.printf("[TOUCH]   → GT911 @ 0x%02X\n", addr);
      }
    }
  }
  if (found == 0) {
    Serial.println("[TOUCH]   NESSUN dispositivo I2C — verifica SDA/SCL/alim.");
  }
  if (s_gt911_addr == 0x00) {
    s_gt911_addr = TOUCH_I2C_ADDR;
    Serial.printf("[TOUCH]   GT911 non trovato — fallback 0x%02X\n", s_gt911_addr);
  }
}

static bool gt911_verify() {
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_CFG >> 8));
  Wire.write((uint8_t)(GT911_REG_CFG & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)1);
  if (!Wire.available()) return false;
  uint8_t cfg_ver = Wire.read();
  Serial.printf("[TOUCH] verify OK cfg=0x%02X\n", cfg_ver);
  return true;
}

static void gt911_init() {
  Serial.printf("[TOUCH] GT911 init  RST=GPIO%d INT=GPIO%d SDA=GPIO%d SCL=GPIO%d\n",
                TOUCH_RST, TOUCH_INT, TOUCH_SDA, TOUCH_SCL);

  // Reset INT=LOW → forza addr 0x5D
  if (TOUCH_INT >= 0) {
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW);
  }
  if (TOUCH_RST >= 0) {
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(20);
    digitalWrite(TOUCH_RST, HIGH);
    delay(10);
  }
  if (TOUCH_INT >= 0) pinMode(TOUCH_INT, INPUT);
  delay(100);

  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000UL);
  delay(20);

  gt911_i2c_scan();
  bool ok = gt911_verify();
  Serial.printf("[TOUCH] %s @ 0x%02X\n", ok ? "OK" : "ERR", s_gt911_addr);
}

// ----------------------------------------------------------------
//  gt911_read() — read primo touch con throttle 20 ms + watchdog 200 ms
//
//  THROTTLE:
//    Il GT911 aggiorna a ~100 Hz. Limitiamo l'I2C a 50 Hz (20 ms)
//    riutilizzando l'ultimo stato valido tra un poll e l'altro.
//
//  WATCHDOG (BUG-1 FIX):
//    Se la lettura I2C fallisce, il vecchio stato "pressed" non deve
//    persistere oltre GT911_STUCK_WD_MS ms. Superata la soglia,
//    forziamo released anche nel ramo di throttle.
// ----------------------------------------------------------------
static volatile bool     s_last_pressed   = false;
static volatile uint16_t s_last_x         = 0;
static volatile uint16_t s_last_y         = 0;
static volatile uint32_t s_last_valid_ms  = 0;

static bool gt911_read(uint16_t* tx, uint16_t* ty) {
  static uint32_t s_last_read_ms = 0;
  uint32_t now = millis();

  // ── Throttle 20 ms ─────────────────────────────────────────────
  if ((now - s_last_read_ms) < 20) {
    // Watchdog: se da troppo tempo non abbiamo una lettura valida,
    // forza release anche se s_last_pressed era true.
    if (s_last_pressed && (now - s_last_valid_ms) > GT911_STUCK_WD_MS) {
      s_last_pressed = false;
    }
    if (s_last_pressed) { *tx = s_last_x; *ty = s_last_y; return true; }
    return false;
  }
  s_last_read_ms = now;

  // ── Lettura stato ──────────────────────────────────────────────
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_STAT >> 8));
  Wire.write((uint8_t)(GT911_REG_STAT & 0xFF));
  if (Wire.endTransmission(true) != 0) {
    // Errore I2C: non invalidare subito, lascia decidere al watchdog.
    return (s_last_pressed && (now - s_last_valid_ms) <= GT911_STUCK_WD_MS)
           ? (*tx = s_last_x, *ty = s_last_y, true)
           : (s_last_pressed = false, false);
  }
  Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)1);
  if (!Wire.available()) {
    return (s_last_pressed && (now - s_last_valid_ms) <= GT911_STUCK_WD_MS)
           ? (*tx = s_last_x, *ty = s_last_y, true)
           : (s_last_pressed = false, false);
  }
  uint8_t stat = Wire.read();

  // Clear flag per prossima lettura (scrittura 0 su 0x814E)
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_STAT >> 8));
  Wire.write((uint8_t)(GT911_REG_STAT & 0xFF));
  Wire.write((uint8_t)0x00);
  Wire.endTransmission(true);

  // Bit 7 = buffer ready, bit 3-0 = numero punti
  bool ready = (stat & 0x80) != 0;
  uint8_t npts = stat & 0x0F;
  if (!ready || npts == 0) {
    // Lettura valida che conferma "nessun tocco": resetta stato.
    s_last_pressed = false;
    s_last_valid_ms = now;
    return false;
  }

  // ── Lettura coordinate primo punto ─────────────────────────────
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_PT1 >> 8));
  Wire.write((uint8_t)(GT911_REG_PT1 & 0xFF));
  if (Wire.endTransmission(true) != 0) {
    s_last_pressed = false;
    return false;
  }
  Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)8);
  if (Wire.available() < 8) { s_last_pressed = false; return false; }

  uint8_t xl = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t yl = Wire.read();
  uint8_t yh = Wire.read();
  Wire.read(); Wire.read(); Wire.read(); Wire.read();  // scartati

  *tx = (uint16_t)xl | ((uint16_t)xh << 8);
  *ty = (uint16_t)yl | ((uint16_t)yh << 8);

  s_last_pressed   = true;
  s_last_x         = *tx;
  s_last_y         = *ty;
  s_last_valid_ms  = now;

#if defined(LOG_TOUCH_RAW) && LOG_TOUCH_RAW
  Serial.printf("[TOUCH] RAW stat=0x%02X npts=%d x=%d y=%d\n", stat, npts, *tx, *ty);
#endif
  return true;
}

// ================================================================
//  BUFFER LVGL — PARTIAL REFRESH, SRAM INTERNA
// ================================================================
//
//  Dimensionamento:
//    LCD_H_RES × N_ROWS × sizeof(lv_color_t)
//    = 480 × 28 × 2 = 26.880 byte per buffer
//    × 2 buffer    = 53.760 byte (~52 KB)
//
//  N_ROWS=28 → ~1/10 dello schermo (272/28 ≈ 9.7).
//  Con LV_DISP_DEF_REFR_PERIOD=20 ms e dirty areas tipiche di 1-3
//  widget per frame il budget di flush resta sotto i 4 ms.
//
//  Con 2 buffer (double buffering) LVGL può renderizzare su uno
//  mentre il flush del precedente è in corso → utilizzo CPU-bound
//  del rendering invece di I/O-bound.
// ================================================================
#define DRAW_BUF_ROWS   28
#define DRAW_BUF_PIXELS (LCD_H_RES * DRAW_BUF_ROWS)

static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t* s_dbuf1 = nullptr;
static lv_color_t* s_dbuf2 = nullptr;

// ================================================================
//  FLUSH CALLBACK — Arduino_GFX blocking write
//  Identico a prima: draw16bitRGBBitmap copia solo l'area richiesta
//  (partial). Con full_refresh=0 l'area coincide con le dirty regions
//  di LVGL, non con tutto il framebuffer.
// ================================================================
static void lv_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  s_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)&color_p->full, w, h);
  lv_disp_flush_ready(drv);
}

// ================================================================
//  TOUCH CALLBACK LVGL
// ================================================================
static void lv_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  uint16_t x = 0, y = 0;
  if (gt911_read(&x, &y)) {
    // Display ruotato 180° → mirror coordinate
    x = LCD_H_RES - 1 - x;
    y = LCD_V_RES - 1 - y;
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
//  BACKLIGHT PWM
// ================================================================
static void backlight_init(uint8_t brightness) {
  ledcAttach(LCD_BL, 44100, 8);
  ledcWrite(LCD_BL, brightness);
  Serial.printf("[DISP] Backlight GPIO%d brightness=%d\n", LCD_BL, brightness);
}

// ================================================================
//  display_init() — chiamato UNA SOLA VOLTA da setup()
//  PRECONDIZIONE: Serial.begin() già chiamato.
// ================================================================
void display_init() {
  Serial.println("========================================");
  Serial.println("[DISP] display_init() v24 PARTIAL START");

  // ── 1. Bus QSPI + Panel NV3041A ─────────────────────────────
  Serial.printf("[DISP] NV3041A init  CS=%d CLK=%d D0=%d D1=%d D2=%d D3=%d\n",
                LCD_CS, LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);

  s_bus = new Arduino_ESP32QSPI(LCD_CS, LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
  s_gfx = new Arduino_NV3041A(s_bus, GFX_NOT_DEFINED, 2 /*rot180*/, true /*IPS*/);

  if (!s_gfx->begin(20000000UL)) {      // 20 MHz — invariato (richiesta utente)
    Serial.println("[DISP] ERR begin() — verifica QSPI wiring");
  } else {
    Serial.printf("[DISP] NV3041A OK  %dx%d\n", s_gfx->width(), s_gfx->height());
  }
  s_gfx->fillScreen(BLACK);
  backlight_init(200);

  // ── 2. Touch GT911 ───────────────────────────────────────────
  gt911_init();

  // ── 3. Log memoria PRE-LVGL ──────────────────────────────────
  size_t sram_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  Serial.printf("[MEM] pre-LVGL  SRAM=%u  PSRAM=%uKB\n",
                (unsigned)sram_free, (unsigned)(psram_free / 1024));
  if (psram_free == 0) {
    Serial.println("[MEM] WARN: PSRAM non rilevata — LV_MEM_CUSTOM PSRAM fallirà");
  }

  // ── 4. DRAW BUFFERS — SRAM interna, partial refresh ─────────
  const size_t dbuf_bytes = (size_t)DRAW_BUF_PIXELS * sizeof(lv_color_t);
  Serial.printf("[DISP] Draw buffers SRAM: 2 × %uB (1/%.1f schermo)\n",
                (unsigned)dbuf_bytes,
                (float)LCD_V_RES / DRAW_BUF_ROWS);

  s_dbuf1 = (lv_color_t*)heap_caps_malloc(
                 dbuf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  s_dbuf2 = (lv_color_t*)heap_caps_malloc(
                 dbuf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (!s_dbuf1 || !s_dbuf2) {
    // Fallback: un solo buffer più piccolo in SRAM se non c'è spazio.
    Serial.println("[DISP] WARN: SRAM insufficiente — fallback singolo buffer 10 righe");
    if (s_dbuf1) { heap_caps_free(s_dbuf1); s_dbuf1 = nullptr; }
    if (s_dbuf2) { heap_caps_free(s_dbuf2); s_dbuf2 = nullptr; }
    static lv_color_t fb_fallback[LCD_H_RES * 10];
    lv_disp_draw_buf_init(&s_draw_buf, fb_fallback, nullptr, LCD_H_RES * 10);
  } else {
    lv_disp_draw_buf_init(&s_draw_buf, s_dbuf1, s_dbuf2, DRAW_BUF_PIXELS);
    Serial.printf("[DISP] Draw buffers OK  2 × %u px  SRAM interna\n",
                  (unsigned)DRAW_BUF_PIXELS);
  }

  // ── 5. LVGL init ─────────────────────────────────────────────
  lv_init();
  Serial.println("[DISP] lv_init() OK");

  // ── 6. Display driver — PARTIAL REFRESH ─────────────────────
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res      = LCD_H_RES;
  disp_drv.ver_res      = LCD_V_RES;
  disp_drv.flush_cb     = lv_flush_cb;
  disp_drv.draw_buf     = &s_draw_buf;
  disp_drv.full_refresh = 0;                  // ← PARTIAL REFRESH
  // Direct mode off: necessario per partial refresh con DB swap
  disp_drv.direct_mode  = 0;
  lv_disp_drv_register(&disp_drv);
  Serial.printf("[DISP] LVGL partial-refresh driver OK  %dx%d\n",
                LCD_H_RES, LCD_V_RES);

  // ── 7. Touch driver ──────────────────────────────────────────
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touch_cb;
  lv_indev_drv_register(&indev_drv);

  // ── 8. Log memoria POST ──────────────────────────────────────
  Serial.printf("[MEM] post-init  SRAM=%u  PSRAM=%uKB\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  Serial.println("[DISP] display_init() v24 DONE");
  Serial.println("========================================");
}
