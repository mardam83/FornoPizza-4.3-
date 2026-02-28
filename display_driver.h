/**
 * display_driver.h — LovyanGFX + LVGL per JC4827W543C
 *
 * Display:  JC4827W543C  480×272  ST7701S  SPI 4-wire
 * Driver:   LovyanGFX  Panel_ST7701S  Bus_SPI
 *
 * LIBRERIE NECESSARIE (Library Manager):
 *   - LovyanGFX  >= 1.1.12
 *   - lvgl        >= 8.3.0
 *
 * NOTE:
 *   Il ST7701S è un controller SPI — niente RGB parallelo.
 *   LovyanGFX gestisce il protocollo di init e il trasferimento SPI.
 *   Il backlight è su GPIO45 (PWM LEDC canale 0).
 *
 *   Buffer LVGL: usiamo 1/10 dello schermo in SRAM interna
 *   (480 * 28 * 2 ≈ 26 KB — ben dentro i 512 KB di SRAM di S3).
 */

#pragma once
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "hardware.h"

// ================================================================
//  CONFIGURAZIONE LOVYANGFX — ST7701S  SPI
// ================================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7701S   _panel_instance;
  lgfx::Bus_SPI         _bus_instance;
  lgfx::Light_PWM       _light_instance;

public:
  LGFX() {
    // ---- Bus SPI ----
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host    = SPI2_HOST;   // SPI2 = FSPI su S3 (GPIO 4-16 safe)
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;    // 40 MHz — ST7701S max ~50 MHz
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk    = LCD_CLK;
      cfg.pin_mosi    = LCD_MOSI;
      cfg.pin_miso    = -1;          // non usato
      cfg.pin_dc      = LCD_DC;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // ---- Panel ST7701S ----
    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs      = LCD_CS;
      cfg.pin_rst     = LCD_RST;
      cfg.pin_busy    = -1;

      cfg.panel_width   = LCD_H_RES;
      cfg.panel_height  = LCD_V_RES;
      cfg.memory_width  = LCD_H_RES;
      cfg.memory_height = LCD_V_RES;

      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;

      cfg.invert       = false;
      cfg.rgb_order    = false;      // BGR — verifica col tuo pannello
      cfg.dlen_16bit   = false;
      cfg.bus_shared   = false;      // SPI dedicato al display

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
//  LVGL — DRAW BUFFER
//  1/10 dello schermo in SRAM interna (più veloce della PSRAM)
// ================================================================
#define LVGL_BUF_LINES  28   // 480 * 28 * 2 = 26.880 byte ≈ 26 KB

static lv_disp_draw_buf_t draw_buf;
static lv_color_t         buf1[LCD_H_RES * LVGL_BUF_LINES];

// ================================================================
//  CALLBACK LVGL → LovyanGFX flush
// ================================================================
static void lv_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  gfx.startWrite();
  gfx.setAddrWindow(area->x1, area->y1, w, h);
  gfx.writePixels((lgfx::rgb565_t*)color_p, w * h);
  gfx.endWrite();

  lv_disp_flush_ready(drv);
}

// ================================================================
//  display_init() — chiama da setup() PRIMA di touch_init()
// ================================================================
inline void display_init() {
  // Inizializza LovyanGFX (gestisce RST e init ST7701S internamente)
  gfx.init();
  gfx.setRotation(0);
  gfx.setBrightness(200);   // 0-255
  gfx.fillScreen(TFT_BLACK);

  // Inizializza LVGL
  lv_init();

  // Draw buffer in SRAM interna
  lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, LCD_H_RES * LVGL_BUF_LINES);

  // Registra driver display
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = LCD_H_RES;
  disp_drv.ver_res  = LCD_V_RES;
  disp_drv.flush_cb = lv_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  Serial.printf("[DISP] ST7701S Init OK %dx%d\n", LCD_H_RES, LCD_V_RES);
}
