/**
 * display_driver.h — LovyanGFX + LVGL per JC4827W543C
 *
 * Display:  JC4827W543C  480×272  ST7701S  SPI 4-wire
 * MCU:      ESP32-S3-WROOM-1-N4R8 (8MB PSRAM OPI)
 *
 * FIX: #define LGFX_USE_V1 deve precedere l'include di LovyanGFX.hpp
 *      affinché Panel_ST7701S (e tutti i panel V1) siano disponibili.
 *
 * BUFFER: Double full framebuffer in PSRAM + DMA SPI non-bloccante.
 *   2 × (480 × 272 × 2) = 522.240 byte ≈ 510 KB in PSRAM OPI
 */

#pragma once

// ── OBBLIGATORIO prima di LovyanGFX.hpp per abilitare i panel V1 ──
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <LGFX_JC4827W543_4.3inch_ESP32S3.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "hardware.h"

// ================================================================
//  CONFIGURAZIONE LOVYANGFX — ST7701S  SPI
// ================================================================

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7701S  _panel_instance;
  lgfx::Bus_SPI        _bus_instance; // O quello che hai ora
  lgfx::Light_PWM      _light_instance;

public:
  LGFX() {
    // ---- Bus SPI ----
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = LCD_CLK;
      cfg.pin_mosi    = LCD_MOSI;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = LCD_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // ---- Panel ST7701S ----
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = LCD_CS;
      cfg.pin_rst          = LCD_RST;
      cfg.pin_busy         = -1;
      cfg.panel_width      = LCD_H_RES;
      cfg.panel_height     = LCD_V_RES;
      cfg.memory_width     = LCD_H_RES;
      cfg.memory_height    = LCD_V_RES;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel_instance.config(cfg);
    }

    // ---- Backlight PWM ----
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl      = LCD_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 0;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};

// ================================================================
//  ISTANZA GLOBALE (definita in FornoPizza_S3.ino)
// ================================================================
extern LGFX gfx;

// ================================================================
//  LVGL — DOUBLE FULL FRAMEBUFFER in PSRAM OPI
//  480 × 272 × 2 byte = 261.120 byte × 2 = 522.240 byte ≈ 510 KB
// ================================================================
#define LVGL_FB_SIZE  (LCD_H_RES * LCD_V_RES)

static lv_disp_draw_buf_t draw_buf;
static lv_color_t*        fb1 = nullptr;
static lv_color_t*        fb2 = nullptr;

// ================================================================
//  CALLBACK LVGL → LovyanGFX flush — DMA non-bloccante
// ================================================================
static void lv_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx.startWrite();
  gfx.setAddrWindow(area->x1, area->y1, w, h);
  gfx.writePixelsDMA((lgfx::rgb565_t*)color_p, w * h);
  gfx.waitDMA();
  gfx.endWrite();
  lv_disp_flush_ready(drv);
}

// ================================================================
//  display_init()
// ================================================================
inline void display_init() {
  fb1 = (lv_color_t*)heap_caps_malloc(LVGL_FB_SIZE * sizeof(lv_color_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  fb2 = (lv_color_t*)heap_caps_malloc(LVGL_FB_SIZE * sizeof(lv_color_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!fb1 || !fb2) {
    Serial.println("[DISP] ERRORE PSRAM! Fallback SRAM 26KB");
    static lv_color_t fallback_buf[LCD_H_RES * 28];
    lv_disp_draw_buf_init(&draw_buf, fallback_buf, nullptr, LCD_H_RES * 28);
  } else {
    Serial.printf("[DISP] Double FB PSRAM OK: 2 x %u byte\n",
                  (unsigned)(LVGL_FB_SIZE * sizeof(lv_color_t)));
    lv_disp_draw_buf_init(&draw_buf, fb1, fb2, LVGL_FB_SIZE);
  }

  gfx.init();
  gfx.setRotation(0);
  gfx.setBrightness(200);
  gfx.fillScreen(TFT_BLACK);

  lv_init();

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res      = LCD_H_RES;
  disp_drv.ver_res      = LCD_V_RES;
  disp_drv.flush_cb     = lv_flush_cb;
  disp_drv.draw_buf     = &draw_buf;
  disp_drv.full_refresh = 1;
  lv_disp_drv_register(&disp_drv);

  Serial.printf("[DISP] ST7701S Init OK %dx%d — DMA + double FB PSRAM\n",
                LCD_H_RES, LCD_V_RES);
}
