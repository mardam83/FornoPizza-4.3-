/**
 * display_driver.h — LovyanGFX + LVGL per JC4827W543C  v4
 *
 * Display:  JC4827W543C  480×272
 * MCU:      ESP32-S3-WROOM-1-N16R8  (8MB PSRAM OPI)
 *
 * ════════════════════════════════════════════════════════════════
 *  SELEZIONE MODALITÀ DISPLAY
 *
 *  Questo display esiste in due varianti HW con bus diversi:
 *
 *  Modalità A — RGB parallelo (16-bit) + touch GT911/CST820
 *    → Driver: Bus_RGB + Panel_RGB  (LovyanGFX qualsiasi versione)
 *    → Pinout: 16 data + HSYNC/VSYNC/DE/PCLK
 *    → Identificazione: nel tuo LGFX_JC4827W543.h vedi Bus_RGB
 *
 *  Modalità B — SPI 4-wire + touch CST820
 *    → Driver: Bus_SPI + Panel_ST7701S  (LovyanGFX >= 1.1.7)
 *    → Pinout: MOSI/CLK/CS/DC/RST/BL (6 fili)
 *    → Identificazione: nel tuo LGFX_JC4827W543.h vedi Bus_SPI
 *
 *  ⚠ CONFIGURA LA TUA MODALITÀ in debug_config.h:
 *     #define DISPLAY_MODE_RGB  1   ← RGB parallelo (default se errori Panel_ST7701)
 *     #define DISPLAY_MODE_RGB  0   ← SPI ST7701S
 * ════════════════════════════════════════════════════════════════
 */

#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "hardware.h"
#include "debug_config.h"

// ================================================================
//  MODALITÀ DEFAULT: se non definita in debug_config.h usa RGB
//  (perché Panel_ST7701S causa errori su versioni LovyanGFX < 1.1.7)
// ================================================================
#ifndef DISPLAY_MODE_RGB
  #define DISPLAY_MODE_RGB  1
#endif

// ================================================================
//  BOUNCE BUFFER DMA (solo modalità SPI — in RGB non serve)
// ================================================================
#if !DISPLAY_MODE_RGB
  #define DMA_LINES  8
  static DRAM_ATTR __attribute__((aligned(4)))
         uint16_t s_dma_buf[DMA_LINES * LCD_H_RES];
#endif

// ================================================================
//  CLASSE LGFX
// ================================================================
class LGFX : public lgfx::LGFX_Device {

#if DISPLAY_MODE_RGB
  // ── Modalità A: RGB parallelo ────────────────────────────────
  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;
  lgfx::Light_PWM   _light_instance;
  lgfx::Touch_CST820 _touch_instance;   // CST820 I2C (JC4827W543C)
  // Se hai GT911 invece di CST820: usa lgfx::Touch_GT911

#else
  // ── Modalità B: SPI 4-wire ───────────────────────────────────
  lgfx::Panel_ST7701S  _panel_instance;
  lgfx::Bus_SPI        _bus_instance;
  lgfx::Light_PWM      _light_instance;
  lgfx::Touch_CST820   _touch_instance;

#endif

public:
  LGFX() {

#if DISPLAY_MODE_RGB
    // ════════════════════════════════════════════════════════════
    //  MODALITÀ A — RGB PARALLELO
    //  Pinout JC4827W543C (da LGFX_JC4827W543.h originale)
    // ════════════════════════════════════════════════════════════

    // ---- Pannello RGB ----
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = LCD_H_RES;
      cfg.memory_height = LCD_V_RES;
      cfg.panel_width   = LCD_H_RES;
      cfg.panel_height  = LCD_V_RES;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config_detail();
      cfg.use_psram = 1;
      _panel_instance.config_detail(cfg);
    }

    // ---- Bus RGB 16-bit ----
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;

      // Pin dati RGB565 (da schematica JC4827W543C)
      cfg.pin_d0  = LCD_RGB_B0;   // B0
      cfg.pin_d1  = LCD_RGB_B1;   // B1
      cfg.pin_d2  = LCD_RGB_B2;   // B2
      cfg.pin_d3  = LCD_RGB_B3;   // B3
      cfg.pin_d4  = LCD_RGB_B4;   // B4
      cfg.pin_d5  = LCD_RGB_G0;   // G0
      cfg.pin_d6  = LCD_RGB_G1;   // G1
      cfg.pin_d7  = LCD_RGB_G2;   // G2
      cfg.pin_d8  = LCD_RGB_G3;   // G3
      cfg.pin_d9  = LCD_RGB_G4;   // G4
      cfg.pin_d10 = LCD_RGB_G5;   // G5
      cfg.pin_d11 = LCD_RGB_R0;   // R0
      cfg.pin_d12 = LCD_RGB_R1;   // R1
      cfg.pin_d13 = LCD_RGB_R2;   // R2
      cfg.pin_d14 = LCD_RGB_R3;   // R3
      cfg.pin_d15 = LCD_RGB_R4;   // R4

      cfg.pin_henable = LCD_RGB_DE;
      cfg.pin_vsync   = LCD_RGB_VSYNC;
      cfg.pin_hsync   = LCD_RGB_HSYNC;
      cfg.pin_pclk    = LCD_RGB_PCLK;
      cfg.freq_write  = 14000000;

      // Timing standard JC4827W543C 480×272
      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch  = 43;
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch  = 12;
      cfg.pclk_idle_high    = 1;

      _bus_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);

    // ---- Backlight ----
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl      = LCD_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 0;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    // ---- Touch CST820 I2C ----
    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 0;
      cfg.x_max      = LCD_H_RES - 1;
      cfg.y_min      = 0;
      cfg.y_max      = LCD_V_RES - 1;
      cfg.pin_int    = TOUCH_INT;
      cfg.pin_rst    = TOUCH_RST;
      cfg.i2c_port   = 0;
      cfg.i2c_addr   = TOUCH_I2C_ADDR;
      cfg.pin_sda    = TOUCH_SDA;
      cfg.pin_scl    = TOUCH_SCL;
      cfg.freq       = 400000;
      cfg.offset_rotation = 0;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

#else
    // ════════════════════════════════════════════════════════════
    //  MODALITÀ B — SPI 4-wire + Panel_ST7701S
    // ════════════════════════════════════════════════════════════

    // ---- Bus SPI ----
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 8000000;
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

    // ---- Backlight ----
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl      = LCD_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 0;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    // ---- Touch CST820 I2C ----
    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 0;
      cfg.x_max      = LCD_H_RES - 1;
      cfg.y_min      = 0;
      cfg.y_max      = LCD_V_RES - 1;
      cfg.pin_int    = TOUCH_INT;
      cfg.pin_rst    = TOUCH_RST;
      cfg.i2c_port   = 0;
      cfg.i2c_addr   = TOUCH_I2C_ADDR;
      cfg.pin_sda    = TOUCH_SDA;
      cfg.pin_scl    = TOUCH_SCL;
      cfg.freq       = 400000;
      cfg.offset_rotation = 0;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

#endif // DISPLAY_MODE_RGB

    setPanel(&_panel_instance);
  }
};

// ================================================================
//  ISTANZA GLOBALE
// ================================================================
LGFX lcd;

// ================================================================
//  FRAMEBUFFER IN PSRAM
// ================================================================
static lv_disp_draw_buf_t  s_draw_buf;
static lv_color_t*         s_fb1 = nullptr;
static lv_color_t*         s_fb2 = nullptr;

// ================================================================
//  FLUSH CALLBACK
// ================================================================
static void lv_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  const int32_t x1 = area->x1, y1 = area->y1;
  const int32_t w  = area->x2 - x1 + 1;
  const int32_t h  = area->y2 - area->y1 + 1;

  lcd.startWrite();
  lcd.setAddrWindow(x1, y1, w, h);

#if DISPLAY_MODE_RGB
  // RGB parallelo: scrittura diretta (nessun vincolo DMA/PSRAM)
  lcd.writePixels((lgfx::rgb565_t*)color_p, w * h, false);
#else
  // SPI: bounce buffer DRAM → DMA (PSRAM non DMA-safe direttamente)
  const uint16_t* src = (const uint16_t*)color_p;
  int32_t rows_left   = h;
  int32_t row_offset  = 0;
  while (rows_left > 0) {
    const int32_t chunk = (rows_left < DMA_LINES) ? rows_left : DMA_LINES;
    const int32_t npix  = w * chunk;
    memcpy(s_dma_buf, src + row_offset * w, (size_t)npix * sizeof(uint16_t));
    lcd.writePixelsDMA((lgfx::rgb565_t*)s_dma_buf, npix);
    lcd.waitDMA();
    row_offset += chunk;
    rows_left  -= chunk;
  }
#endif

  lcd.endWrite();
  lv_disp_flush_ready(drv);
}

// ================================================================
//  TOUCH CALLBACK
// ================================================================
static void lv_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  uint16_t tx, ty;
  if (lcd.getTouch(&tx, &ty)) {
    data->point.x = (lv_coord_t)tx;
    data->point.y = (lv_coord_t)ty;
    data->state   = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ================================================================
//  display_init()
// ================================================================
inline void display_init() {

  // ── 1. Hardware ──
  lcd.init();
  lcd.setRotation(0);   // RGB parallelo: 0 = landscape nativo
  lcd.fillScreen(TFT_BLACK);
  lcd.setBrightness(200);

  Serial.printf("[DISP] Modalità: %s\n",
    DISPLAY_MODE_RGB ? "RGB parallelo" : "SPI ST7701S");

  // ── 2. Log SRAM prima dell'alloc ──
  Serial.printf("[MEM] SRAM prima alloc: heap=%u PSRAM=%uKB\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

  const size_t fb_bytes = (size_t)LCD_H_RES * LCD_V_RES * sizeof(lv_color_t);

  // ── 3. Double framebuffer in PSRAM ──
  s_fb1 = (lv_color_t*)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  s_fb2 = (lv_color_t*)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!s_fb1 || !s_fb2) {
    Serial.println("[DISP] WARN: PSRAM non disponibile — fallback SRAM");
    if (s_fb1) { heap_caps_free(s_fb1); s_fb1 = nullptr; }
    if (s_fb2) { heap_caps_free(s_fb2); s_fb2 = nullptr; }
#if DISPLAY_MODE_RGB
    // RGB: usa buffer statico (Panel_RGB gestisce il suo framebuffer interno)
    static lv_color_t fallback_buf[LCD_H_RES * 10];
    lv_disp_draw_buf_init(&s_draw_buf, fallback_buf, nullptr, LCD_H_RES * 10);
#else
    static lv_color_t fallback_buf[LCD_H_RES * DMA_LINES * 2];
    lv_disp_draw_buf_init(&s_draw_buf, fallback_buf, nullptr, LCD_H_RES * DMA_LINES * 2);
#endif
    Serial.println("[DISP] Fallback buffer SRAM attivo");
  } else {
    lv_disp_draw_buf_init(&s_draw_buf, s_fb1, s_fb2,
                          (uint32_t)LCD_H_RES * LCD_V_RES);
    Serial.printf("[DISP] Double FB PSRAM: 2 × %uKB\n",
                  (unsigned)(fb_bytes / 1024));
  }

  // ── 4. LVGL init ──
  lv_init();

  // ── 5. Display driver ──
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res      = LCD_H_RES;
  disp_drv.ver_res      = LCD_V_RES;
  disp_drv.flush_cb     = lv_flush_cb;
  disp_drv.draw_buf     = &s_draw_buf;
  disp_drv.full_refresh = (s_fb1 != nullptr) ? 1 : 0;
  lv_disp_drv_register(&disp_drv);

  // ── 6. Touch driver ──
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touch_cb;
  lv_indev_drv_register(&indev_drv);

  Serial.printf("[MEM] SRAM post-init: heap=%u PSRAM=%uKB\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  Serial.printf("[DISP] OK %dx%d full_refresh=%d\n",
    LCD_H_RES, LCD_V_RES, (s_fb1 != nullptr));
}
