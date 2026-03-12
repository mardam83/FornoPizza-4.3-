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
 *    Touch INT : GPIO3   ← durante reset determina l'indirizzo I2C
 *    Touch RST : GPIO38
 *
 *  GT911 INDIRIZZO I2C:
 *    INT=LOW  durante reset → 0x5D
 *    INT=HIGH durante reset → 0x14
 *    La sequenza gt911_init() forza INT=LOW → atteso 0x5D
 *    Se TOUCH_I2C_ADDR in hardware.h è 0x14, cambiarlo a 0x5D
 *    oppure invertire la logica del pin INT nella sequenza reset.
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
//  Protocollo I2C GT911: registro a 16-bit, MSB first
// ================================================================
#define GT911_REG_STAT   0x814E
#define GT911_REG_PT1    0x8150
#define GT911_REG_CFG    0x8047   // config version — usato per verify
#define GT911_ADDR_0x14  0x14
#define GT911_ADDR_0x5D  0x5D

// Indirizzo effettivo rilevato dall'I2C scan
static uint8_t s_gt911_addr = 0x00;  // 0x00 = non trovato

// ----------------------------------------------------------------
//  gt911_i2c_scan() — verifica quale indirizzo risponde
//  Chiamato dentro gt911_init() dopo Wire.begin()
// ----------------------------------------------------------------
static void gt911_i2c_scan() {
  Serial.println("[TOUCH] I2C scan...");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("[TOUCH]   trovato dispositivo I2C a 0x%02X\n", addr);
      found++;
      if (addr == GT911_ADDR_0x14 || addr == GT911_ADDR_0x5D) {
        s_gt911_addr = addr;
        Serial.printf("[TOUCH]   → GT911 identificato a 0x%02X\n", addr);
      }
    }
  }
  if (found == 0) {
    Serial.println("[TOUCH]   NESSUN dispositivo I2C trovato!");
    Serial.println("[TOUCH]   Verifica cablaggio SDA/SCL e alimentazione.");
  }
  if (s_gt911_addr == 0x00) {
    // Nessun indirizzo GT911 trovato — usa quello da hardware.h come fallback
    s_gt911_addr = TOUCH_I2C_ADDR;
    Serial.printf("[TOUCH]   GT911 NON trovato! Uso fallback addr=0x%02X da hardware.h\n",
                  s_gt911_addr);
  }
}

// ----------------------------------------------------------------
//  gt911_verify() — legge config version per confermare comunicazione
// ----------------------------------------------------------------
static bool gt911_verify() {
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_CFG >> 8));
  Wire.write((uint8_t)(GT911_REG_CFG & 0xFF));
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) {
    Serial.printf("[TOUCH] verify: endTransmission errore %d\n", err);
    return false;
  }
  Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)1);
  if (!Wire.available()) {
    Serial.println("[TOUCH] verify: nessun dato ricevuto");
    return false;
  }
  uint8_t cfg_ver = Wire.read();
  Serial.printf("[TOUCH] verify OK — config_version=0x%02X\n", cfg_ver);
  return true;
}

// ----------------------------------------------------------------
//  gt911_init() — reset hardware + Wire.begin + scan + verify
//
//  SEQUENZA RESET GT911 (datasheet §2.2):
//    L'indirizzo I2C è determinato dal livello su INT durante il reset.
//    INT=LOW  durante il periodo di reset → indirizzo 0x5D
//    INT=HIGH durante il periodo di reset → indirizzo 0x14
//
//    Qui forziamo INT=LOW → atteso addr 0x5D
//    Se il tuo hardware.h ha TOUCH_I2C_ADDR=0x14 e lo scan non trova 0x14,
//    cambia TOUCH_I2C_ADDR a 0x5D in hardware.h
// ----------------------------------------------------------------
static void gt911_init() {
  Serial.printf("[TOUCH] GT911 init — RST=GPIO%d  INT=GPIO%d  SDA=GPIO%d  SCL=GPIO%d\n",
                TOUCH_RST, TOUCH_INT, TOUCH_SDA, TOUCH_SCL);
  Serial.printf("[TOUCH] TOUCH_I2C_ADDR configurato: 0x%02X\n", TOUCH_I2C_ADDR);

  // ── Sequenza reset con INT basso → forza addr 0x5D ──────────
  // INT come output, tenuto LOW prima e durante il reset
  if (TOUCH_INT >= 0) {
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW);
  }

  if (TOUCH_RST >= 0) {
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(20);                    // tRESET ≥ 10ms (datasheet)
    digitalWrite(TOUCH_RST, HIGH);
    delay(10);                    // attendi prima di rilasciare INT
  }

  // Rilascia INT come input dopo il reset
  if (TOUCH_INT >= 0) {
    pinMode(TOUCH_INT, INPUT);
    // Pullup interno opzionale — se INT è flottante potrebbe dare falsi positive
    // pinMode(TOUCH_INT, INPUT_PULLUP);
  }

  delay(100);   // GT911 ha bisogno di ~50-100ms per boot completo dopo reset

  // ── Wire.begin ───────────────────────────────────────────────
  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000UL);
  delay(20);
  Serial.printf("[TOUCH] Wire.begin SDA=%d SCL=%d @ 400kHz\n", TOUCH_SDA, TOUCH_SCL);

  // ── Scan I2C ─────────────────────────────────────────────────
  gt911_i2c_scan();

  // ── Verifica comunicazione ───────────────────────────────────
  bool ok = gt911_verify();
  if (ok) {
    Serial.printf("[TOUCH] GT911 OK  addr=0x%02X\n", s_gt911_addr);
  } else {
    Serial.printf("[TOUCH] GT911 ERRORE comunicazione a 0x%02X\n", s_gt911_addr);
    Serial.println("[TOUCH] Possibili cause:");
    Serial.println("[TOUCH]   1. Indirizzo sbagliato (prova 0x5D vs 0x14 in hardware.h)");
    Serial.println("[TOUCH]   2. SDA/SCL invertiti (swap GPIO8/GPIO4 in hardware.h)");
    Serial.println("[TOUCH]   3. RST tenuto basso (verifica che TOUCH_RST sia HIGH ora)");
    Serial.println("[TOUCH]   4. Alimentazione 2.8V touch non attiva");
  }
}

// ----------------------------------------------------------------
//  gt911_read() — legge coordinate primo touch
//  Ritorna true se tocco valido, false altrimenti
//
//  THROTTLE: il GT911 aggiorna il buffer a ~100Hz (ogni 10ms).
//  Limitiamo le letture I2C a 20ms per non saturare il bus,
//  ma manteniamo l'ultimo stato (PRESSED) tra una lettura e l'altra
//  così LVGL non perde tocchi brevi.
//
//  NOTA Wire: usiamo sempre endTransmission(true) = STOP esplicito
//  prima di ogni requestFrom, per garantire che il bus sia libero.
//  Il repeated-start (false) causa problemi su alcuni driver ESP32.
// ----------------------------------------------------------------
static bool gt911_read(uint16_t* tx, uint16_t* ty) {
  if (s_gt911_addr == 0x00) return false;

  // Throttle: non interrogare il GT911 più spesso di ogni 20ms
  static uint32_t s_last_read_ms = 0;
  static bool     s_last_pressed  = false;
  static uint16_t s_last_x = 0, s_last_y = 0;

  uint32_t now = millis();
  if (now - s_last_read_ms < 20) {
    // Restituisce l'ultimo stato noto senza interrogare il bus
    if (s_last_pressed) { *tx = s_last_x; *ty = s_last_y; }
    return s_last_pressed;
  }
  s_last_read_ms = now;

  // ── Step 1: leggi registro status (1 byte) ──────────────────
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_STAT >> 8));
  Wire.write((uint8_t)(GT911_REG_STAT & 0xFF));
  if (Wire.endTransmission(true) != 0) {   // STOP esplicito
    s_last_pressed = false;
    return false;
  }
  Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)1);
  if (!Wire.available()) { s_last_pressed = false; return false; }
  uint8_t stat = Wire.read();
  uint8_t npts = stat & 0x0F;

  // ── Step 2: pulisci flag status — sempre, indipendentemente da npts ──
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_STAT >> 8));
  Wire.write((uint8_t)(GT911_REG_STAT & 0xFF));
  Wire.write(0x00);
  Wire.endTransmission(true);

  // bit7=buffer ready, npts>0=almeno un punto
  if (!(stat & 0x80) || npts == 0) {
    s_last_pressed = false;
    return false;
  }

  // ── Step 3: leggi coordinate primo punto (8 byte) ────────────
  Wire.beginTransmission(s_gt911_addr);
  Wire.write((uint8_t)(GT911_REG_PT1 >> 8));
  Wire.write((uint8_t)(GT911_REG_PT1 & 0xFF));
  if (Wire.endTransmission(true) != 0) {   // STOP esplicito
    s_last_pressed = false;
    return false;
  }
  Wire.requestFrom((uint8_t)s_gt911_addr, (uint8_t)8);
  if (Wire.available() < 8) { s_last_pressed = false; return false; }

  // Layout verificato su dump reale JC4827W543C (GT911 firmware):
  // Nessun track_id — il registro 0x8150 inizia direttamente con le coordinate
  // Dump alto-sx:  D4 01 FF 00 1E 00 00 00  → x=0x01D4=468? NO
  // Dump basso-dx: 07 00 14 00 1D 00 00 00
  // b[0]=xl  b[1]=xh  b[2]=yl  b[3]=yh  b[4]=size  b[5..7]=padding
  // Verifica: alto-sx  x=0xD4=212? no — basso-dx x=0x07=7 ✓ alto-sx
  //           basso-dx x=0x07|0x00<<8=7 — non è basso-dx
  // Rileggendo: alto-sx b[0]=D4=212, basso-dx b[0]=07=7
  // Display 480px: basso-dx x atteso ~470, alto-sx x atteso ~10
  // → b[0] è x basso (xl), b[1] è x alto (xh)
  //   alto-sx:  x = 0xD4|0x01<<8 = 468 ✓ (vicino a 480, display ruotato 180°)
  //   basso-dx: x = 0x07|0x00<<8 = 7   ✓ (vicino a 0)
  //   alto-sx:  y = 0xFF|0x00<<8 = 255 ✓ (vicino a 272, display ruotato 180°)
  //   basso-dx: y = 0x14|0x00<<8 = 20  ✓ (vicino a 0)
  // Confermato: xl=b[0], xh=b[1], yl=b[2], yh=b[3], size=b[4], pad=b[5..7]
  uint8_t xl = Wire.read();   // b[0]
  uint8_t xh = Wire.read();   // b[1]
  uint8_t yl = Wire.read();   // b[2]
  uint8_t yh = Wire.read();   // b[3]
  Wire.read();                 // b[4] size
  Wire.read();                 // b[5] padding
  Wire.read();                 // b[6] padding
  Wire.read();                 // b[7] padding

  *tx = (uint16_t)xl | ((uint16_t)xh << 8);
  *ty = (uint16_t)yl | ((uint16_t)yh << 8);

  s_last_pressed = true;
  s_last_x = *tx;
  s_last_y = *ty;

#if defined(LOG_TOUCH_RAW) && LOG_TOUCH_RAW
  Serial.printf("[TOUCH] RAW stat=0x%02X npts=%d x=%d y=%d\n", stat, npts, *tx, *ty);
#endif

  return true;
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
    if (x >= LCD_H_RES) x = LCD_H_RES - 1;
    if (y >= LCD_V_RES) y = LCD_V_RES - 1;
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state   = LV_INDEV_STATE_PR;

    // Log tocco — rimosso in produzione per non saturare seriale
    // Serial.printf("[TOUCH] x=%d y=%d\n", x, y);
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
//  PRECONDIZIONE: Serial.begin() già chiamato prima di questa funzione
// ================================================================
void display_init() {
  Serial.println("========================================");
  Serial.println("[DISP] display_init() START");

  // ── 1. Bus QSPI + Panel NV3041A ─────────────────────────────
  Serial.printf("[DISP] NV3041A init  CS=%d CLK=%d D0=%d D1=%d D2=%d D3=%d\n",
                LCD_CS, LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);

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

  Serial.println("[DISP] s_gfx->begin() ...");
  if (!s_gfx->begin(40000000UL)) {
    Serial.println("[DISP] ERRORE: begin() fallito! Verifica cablaggio QSPI.");
  } else {
    Serial.printf("[DISP] NV3041A OK  %dx%d\n", s_gfx->width(), s_gfx->height());
  }

  s_gfx->fillScreen(BLACK);
  backlight_init(200);

  // ── 2. Touch GT911 ───────────────────────────────────────────
  Serial.println("[DISP] gt911_init() ...");
  gt911_init();

  // ── 3. Log memoria ───────────────────────────────────────────
  size_t sram_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  Serial.printf("[MEM] pre-LVGL  SRAM=%u  PSRAM=%uKB\n",
                (unsigned)sram_free, (unsigned)(psram_free / 1024));

  if (psram_free == 0) {
    Serial.println("[MEM] WARN: PSRAM non rilevata! Verifica N4R8 / PSRAM enabled in board config.");
  }

  // ── 4. Buffer LVGL ───────────────────────────────────────────
  const size_t fb_bytes = (size_t)LCD_H_RES * LCD_V_RES * sizeof(lv_color_t);
  Serial.printf("[DISP] Alloco double FB PSRAM: 2 × %uKB = %uKB\n",
                (unsigned)(fb_bytes / 1024), (unsigned)(2 * fb_bytes / 1024));

  s_fb1 = (lv_color_t*)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  s_fb2 = (lv_color_t*)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!s_fb1 || !s_fb2) {
    Serial.println("[DISP] WARN: PSRAM non disponibile — fallback SRAM 10 righe");
    if (s_fb1) { heap_caps_free(s_fb1); s_fb1 = nullptr; }
    if (s_fb2) { heap_caps_free(s_fb2); s_fb2 = nullptr; }
    static lv_color_t fallback_buf[LCD_H_RES * 10];
    lv_disp_draw_buf_init(&s_draw_buf, fallback_buf, nullptr, LCD_H_RES * 10);
    Serial.println("[DISP] Buffer LVGL: SRAM fallback 10 righe");
  } else {
    lv_disp_draw_buf_init(&s_draw_buf, s_fb1, s_fb2, (uint32_t)LCD_H_RES * LCD_V_RES);
    Serial.printf("[DISP] Buffer LVGL: double FB PSRAM OK  2×%uKB\n",
                  (unsigned)(fb_bytes / 1024));
  }

  // ── 5. LVGL init ─────────────────────────────────────────────
  Serial.println("[DISP] lv_init() ...");
  lv_init();
  Serial.println("[DISP] lv_init() OK");

  // ── 6. Display driver LVGL ───────────────────────────────────
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res      = LCD_H_RES;
  disp_drv.ver_res      = LCD_V_RES;
  disp_drv.flush_cb     = lv_flush_cb;
  disp_drv.draw_buf     = &s_draw_buf;
  disp_drv.full_refresh = (s_fb1 != nullptr) ? 1 : 0;
  lv_disp_drv_register(&disp_drv);
  Serial.printf("[DISP] LVGL disp driver registrato  %dx%d  full_refresh=%d\n",
                LCD_H_RES, LCD_V_RES, disp_drv.full_refresh);

  // ── 7. Touch driver LVGL ─────────────────────────────────────
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touch_cb;
  lv_indev_t* indev = lv_indev_drv_register(&indev_drv);
  Serial.printf("[DISP] LVGL indev touch registrato  ptr=%p\n", (void*)indev);

  // ── 8. Log finale ────────────────────────────────────────────
  Serial.printf("[MEM] post-init  SRAM=%u  PSRAM=%uKB\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  Serial.println("[DISP] display_init() DONE");
  Serial.println("========================================");
}