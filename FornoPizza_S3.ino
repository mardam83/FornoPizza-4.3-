/**
 * ================================================================
 *  FORNO PIZZA CONTROLLER v10.0 — PORTING ESP32-S3
 *  Board  : ESP32-S3 (JC4827W543 o equivalente)
 *  Display: 4.3" ST7701S 480×272 (RGB parallel)
 *  Touch  : GT911 capacitivo (I2C)
 *  Libreria display: LovyanGFX (NON TFT_eSPI, NON Panel_ST7701S standalone)
 *
 *  DIFFERENZE rispetto all'originale (ESP32-2432S028R / ILI9341 320×240):
 *    • Display driver: TFT_eSPI → LovyanGFX LGFX_JC4827W543
 *    • Touch: XPT2046 (SPI resistivo) → GT911 (I2C capacitivo)
 *    • Risoluzione LVGL: 320×240 → 480×272
 *    • Buffer LVGL ridimensionato: 480×25 pixel
 *    • Pin ESP32-S3: vedi sezione PIN
 *    • Backlight: controllato da LovyanGFX (niente TFT_BL_PIN manuale)
 *
 *  FUNZIONALITÀ INVARIATE:
 *    • Dual-core: Core0=LVGL, Core1=PID+Watchdog
 *    • Safety system completo (overtemp, runaway UP/DOWN, WDG, TC error)
 *    • Ventola emergenza sempre ON in shutdown
 *    • PID dual/single, autotune, NVS, WiFi, MQTT, OTA
 *
 *  COME INSTALLARE LovyanGFX:
 *    PlatformIO: lib_deps = lovyan03/LovyanGFX
 *    Arduino IDE: Library Manager → "LovyanGFX"
 *    NON serve un file User_Setup.h — la config è nel codice (vedi lgfx_setup)
 *
 *  TOUCH GT911:
 *    La libreria LovyanGFX gestisce GT911 internamente tramite il
 *    metodo getTouch(). Non serve libreria esterna.
 * ================================================================
 */


#include <Arduino.h>
#include <SPI.h>

// ── Display: tutto in LGFX_JC4827W543.h ─────────────────────────────
// LGFX_USE_V1, #include <LovyanGFX.hpp>, classe LGFX e "lcd" sono
// nel file .h — questo è il pattern degli esempi lgfx_user ufficiali
// (vedi Arduino/libraries/LovyanGFX/src/lgfx_user/*.h)
#include "LGFX_JC4827W543.h"

#include <max6675.h>
#include <PID_v1.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "ui.h"
#include "pid_ctrl.h"
#include "nvs_storage.h"
#include "hardware.h"
#include "wifi_mqtt.h"
#include "autotune.h"
#include "ui_wifi.h"
#include "ota_manager.h"
#include "web_ota.h"



// ================================================================
//  PIN — ESP32-S3
//  Termocoppia MAX6675: SPI software (invariato come logica)
//  Relay: mantieni gli stessi definiti in hardware.h
// ================================================================
// Backlight gestito da LovyanGFX — nessun TFT_BL_PIN manuale


// ================================================================
//  PARAMETRI SICUREZZA (invariati)
// ================================================================
#define TEMP_MAX_SAFE       480.0f
#define FAN_OFF_TEMP        80.0f
#define WDG_TIMEOUT_MS      5000
#define WDG_CHECK_MS        500

#define RUNAWAY_DOWN_ENABLED  1
#define RUNAWAY_DOWN_MS       300000
#define RUNAWAY_MIN_DROP      60.0f
#define RUNAWAY_RECOVERY_DEG  15.0f

#define RUNAWAY_UP_ENABLED    1
#define RUNAWAY_RISE_DEG      40.0f
#define RUNAWAY_RISE_MS       8000

#define TC_READ_TIMEOUT_MS  2000

// ================================================================
//  TASK CONFIG (invariati)
// ================================================================
#define TASK_LVGL_CORE      0
#define TASK_LVGL_PRIO      1
#define TASK_LVGL_STACK     8192

#define TASK_PID_CORE       1
#define TASK_PID_PRIO       2
#define TASK_PID_STACK      4096

#define TASK_WDG_CORE       1
#define TASK_WDG_PRIO       3
#define TASK_WDG_STACK      2048

#define MUTEX_TIMEOUT_MS    10

// ================================================================
//  DISPLAY RESOLUTION
// ================================================================
#define DISP_W  480
#define DISP_H  272

// ================================================================
//  HARDWARE
// ================================================================
// lcd è già dichiarato sopra come LGFX
// NO XPT2046 — il touch è integrato in LGFX (GT911)
MAX6675  tc_base (TC_SCK, TC_CS_BASE,  TC_MISO);
MAX6675  tc_cielo(TC_SCK, TC_CS_CIELO, TC_MISO);
NVSStorage nvs;

// ================================================================
//  SINCRONIZZAZIONE
// ================================================================
SemaphoreHandle_t g_mutex = nullptr;
#define MUTEX_TAKE()  (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE)
#define MUTEX_GIVE()   xSemaphoreGive(g_mutex)

volatile uint32_t g_pid_heartbeat    = 0;
volatile bool     g_emergency_shutdown = false;

// ================================================================
//  STATO GLOBALE (invariato)
// ================================================================
AppState g_state = {
  .temp_base     = 0.0,
  .temp_cielo    = 0.0,
  .set_base      = 250.0,
  .set_cielo     = 300.0,
  .pid_out_base  = 0.0,
  .pid_out_cielo = 0.0,
  .kp_base       = 3.0,  .ki_base  = 0.02, .kd_base  = 2.0,
  .kp_cielo      = 2.5,  .ki_cielo = 0.03, .kd_cielo = 1.5,
  .sensor_mode   = SensorMode::SINGLE,
  .pct_base      = 100,
  .pct_cielo     = 100,
  .base_enabled  = false,
  .cielo_enabled = false,
  .luce_on       = false,
  .relay_base    = false,
  .relay_cielo   = false,
  .fan_on        = false,
  .preheat_base  = false,
  .preheat_cielo = false,
  .tc_base_err   = false,
  .tc_cielo_err  = false,
  .safety_shutdown = false,
  .safety_reason   = SafetyReason::NONE,
  .autotune_status = AutotuneStatus::IDLE,
  .autotune_split  = 50,
  .autotune_cycles = 0,
  .autotune_kp     = 0.0f,
  .autotune_ki     = 0.0f,
  .autotune_kd     = 0.0f,
  .active_screen = Screen::MAIN,
  .nvs_dirty     = false,
};

// ================================================================
//  PID (invariato)
// ================================================================
PIDController pid_base (&g_state.temp_base,  &g_state.pid_out_base,
                         &g_state.set_base,
                         g_state.kp_base, g_state.ki_base, g_state.kd_base);
PIDController pid_cielo(&g_state.temp_cielo, &g_state.pid_out_cielo,
                         &g_state.set_cielo,
                         g_state.kp_cielo, g_state.ki_cielo, g_state.kd_cielo);

// ================================================================
//  BUFFER LVGL — adattato a 480×272
// ================================================================
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         lb1[DISP_W * 25];
static lv_color_t         lb2[DISP_W * 25];

// ================================================================
//  EMERGENCY SHUTDOWN (invariato)
// ================================================================
void IRAM_ATTR emergency_shutdown(SafetyReason reason) {
  RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
  RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);

  g_emergency_shutdown     = true;
  g_state.relay_base       = false;
  g_state.relay_cielo      = false;
  g_state.base_enabled     = false;
  g_state.cielo_enabled    = false;
  g_state.safety_shutdown  = true;
  g_state.safety_reason    = reason;

  const char* reasons[] = {
    "NONE", "TC_ERROR", "OVERTEMP", "RUNAWAY_DOWN",
    "RUNAWAY_UP", "WDG_TIMEOUT"
  };
  int idx = (int)reason;
  if (idx >= 0 && idx <= 5)
    Serial.printf("\n!!! SHUTDOWN SICUREZZA: %s !!!\n", reasons[idx]);
}

// ================================================================
//  LVGL CALLBACKS — aggiornati per LovyanGFX e 480×272
// ================================================================
static void lvgl_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.writePixels((lgfx::rgb565_t*)px, w * h);
  lcd.endWrite();
  lv_disp_flush_ready(drv);
}

static void lvgl_touch(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  uint16_t tx, ty;
  // getTouch() restituisce true se c'è un tocco valido
  if (lcd.getTouch(&tx, &ty)) {
    data->point.x = (lv_coord_t)tx;
    data->point.y = (lv_coord_t)ty;
    data->state   = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ================================================================
//  NVS helpers (invariati)
// ================================================================
static void nvs_load_to_state() {
  NVSData d;
  nvs.load(d);
  g_state.set_base    = d.set_base;
  g_state.set_cielo   = d.set_cielo;
  g_state.kp_base     = d.kp_base;
  g_state.ki_base     = d.ki_base;
  g_state.kd_base     = d.kd_base;
  g_state.kp_cielo    = d.kp_cielo;
  g_state.ki_cielo    = d.ki_cielo;
  g_state.kd_cielo    = d.kd_cielo;
  g_state.sensor_mode = (d.single_mode == 1) ? SensorMode::SINGLE : SensorMode::DUAL;
  g_state.pct_base    = d.pct_base;
  g_state.pct_cielo   = d.pct_cielo;
}

static void nvs_save_from_state() {
  NVSData d;
  if (!MUTEX_TAKE()) return;
  d.set_base    = (float)g_state.set_base;
  d.set_cielo   = (float)g_state.set_cielo;
  d.kp_base     = (float)g_state.kp_base;
  d.ki_base     = (float)g_state.ki_base;
  d.kd_base     = (float)g_state.kd_base;
  d.kp_cielo    = (float)g_state.kp_cielo;
  d.ki_cielo    = (float)g_state.ki_cielo;
  d.kd_cielo    = (float)g_state.kd_cielo;
  d.single_mode = (g_state.sensor_mode == SensorMode::SINGLE) ? 1 : 0;
  d.pct_base    = g_state.pct_base;
  d.pct_cielo   = d.pct_cielo;
  g_state.nvs_dirty = false;
  MUTEX_GIVE();
  nvs.save(d);
}

// ================================================================
//  TASK_WATCHDOG — invariato
// ================================================================
static void Task_Watchdog(void* param) {
  Serial.printf("[Core %d] Task_Watchdog avviato (timeout=%dms)\n",
                xPortGetCoreID(), WDG_TIMEOUT_MS);

  uint32_t last_heartbeat = g_pid_heartbeat;
  uint32_t last_beat_time = millis();

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(WDG_CHECK_MS));

    if (g_emergency_shutdown) {
      RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
      RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
      continue;
    }

    uint32_t now = millis();
    uint32_t hb  = g_pid_heartbeat;

    if (hb != last_heartbeat) {
      last_heartbeat = hb;
      last_beat_time = now;
    } else {
      if (now - last_beat_time >= WDG_TIMEOUT_MS) {
        Serial.printf("[WDG] TIMEOUT! PID heartbeat fermo da %lums\n",
                      now - last_beat_time);
        emergency_shutdown(SafetyReason::WDG_TIMEOUT);
      }
    }
  }
}

// ================================================================
//  TASK_PID — invariato (copia esatta dell'originale)
// ================================================================
static void Task_PID(void* param) {
  Serial.printf("[Core %d] Task_PID avviato\n", xPortGetCoreID());

  unsigned long win_base_single  = 0;
  unsigned long win_cielo_single = 0;
  uint32_t      last_nvs_ms      = millis();

  float    rd_peak_temp       = -1.0f;
  float    rd_valley_temp     = -1.0f;
  uint32_t rd_drop_start_ms   = 0;
  bool     rd_drop_active     = false;

  float    ru_start_temp      = -1.0f;
  uint32_t ru_start_ms        = 0;

  for (;;) {
    uint32_t now = millis();

    if (g_emergency_shutdown) {
      RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
      RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
      bool fan = (g_state.temp_cielo > FAN_OFF_TEMP);
      if (fan != g_state.fan_on) {
        g_state.fan_on = fan;
        RELAY_WRITE(RELAY_FAN, RELAY_FAN_INV, fan);
      }
      g_pid_heartbeat++;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // ── 1. Lettura sensori ──
    uint32_t t_read_start = millis();
    float t_base_raw  = tc_base.readCelsius();
    float t_cielo_raw = tc_cielo.readCelsius();
    uint32_t t_read_ms = millis() - t_read_start;

    bool err_base  = isnan(t_base_raw)  || t_base_raw  <= 0.0f;
    bool err_cielo = isnan(t_cielo_raw) || t_cielo_raw <= 0.0f;

    if (t_read_ms > TC_READ_TIMEOUT_MS) {
      Serial.printf("[PID] TC read timeout: %lums\n", t_read_ms);
      emergency_shutdown(SafetyReason::TC_ERROR);
      vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
      continue;
    }

    // ── 2. Sicurezza ──
    if (err_cielo && (g_state.base_enabled || g_state.cielo_enabled)) {
      emergency_shutdown(SafetyReason::TC_ERROR);
      vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
      continue;
    }

    if (!err_cielo) {
      if (t_cielo_raw > TEMP_MAX_SAFE) {
        Serial.printf("[PID] OVERTEMP: %.1f > %.1f\n", t_cielo_raw, TEMP_MAX_SAFE);
        emergency_shutdown(SafetyReason::OVERTEMP);
        vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
        continue;
      }

      bool relay_on = g_state.relay_base || g_state.relay_cielo;

#if RUNAWAY_DOWN_ENABLED
      if (relay_on) {
        if (rd_peak_temp < 0.0f) {
          rd_peak_temp   = t_cielo_raw;
          rd_valley_temp = t_cielo_raw;
          rd_drop_active = false;
        }
        if (t_cielo_raw > rd_peak_temp) {
          rd_peak_temp = t_cielo_raw;
          if (rd_drop_active && (t_cielo_raw - rd_valley_temp) >= RUNAWAY_RECOVERY_DEG) {
            rd_drop_active = false;
            rd_valley_temp = t_cielo_raw;
            Serial.printf("[PID] Runaway DOWN reset: temp risalita a %.1f\n", t_cielo_raw);
          }
        }
        if (t_cielo_raw < rd_valley_temp)
          rd_valley_temp = t_cielo_raw;
        float total_drop = rd_peak_temp - rd_valley_temp;
        if (total_drop >= RUNAWAY_MIN_DROP) {
          if (!rd_drop_active) {
            rd_drop_active   = true;
            rd_drop_start_ms = now;
            Serial.printf("[PID] Runaway DOWN iniziato: peak=%.1f valley=%.1f drop=%.1f°C\n",
                          rd_peak_temp, rd_valley_temp, total_drop);
          } else if (now - rd_drop_start_ms > RUNAWAY_DOWN_MS) {
            Serial.printf("[PID] RUNAWAY DOWN: %.1f°C di discesa per %lums\n",
                          total_drop, now - rd_drop_start_ms);
            emergency_shutdown(SafetyReason::RUNAWAY_DOWN);
            vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
            continue;
          }
        }
      } else {
        rd_peak_temp   = -1.0f;
        rd_valley_temp = -1.0f;
        rd_drop_active = false;
      }
#endif

#if RUNAWAY_UP_ENABLED
      if (ru_start_temp < 0.0f) {
        ru_start_temp = t_cielo_raw;
        ru_start_ms   = now;
      } else {
        uint32_t elapsed = now - ru_start_ms;
        if (elapsed >= RUNAWAY_RISE_MS) {
          float rise = t_cielo_raw - ru_start_temp;
          if (rise > RUNAWAY_RISE_DEG) {
            Serial.printf("[PID] RUNAWAY UP: +%.1f°C in %lums\n", rise, elapsed);
            emergency_shutdown(SafetyReason::RUNAWAY_UP);
            vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
            continue;
          }
          ru_start_temp = t_cielo_raw;
          ru_start_ms   = now;
        }
      }
#endif
    }

    // ── 3. Autotune / PID ──
    if (autotune_is_running()) {
      autotune_run(t_cielo_raw, now);
      if (MUTEX_TAKE()) {
        g_state.tc_cielo_err = err_cielo;
        if (!err_cielo) g_state.temp_cielo = (double)t_cielo_raw;
        g_state.temp_base = g_state.temp_cielo;
        MUTEX_GIVE();
      }
    } else {
      if (MUTEX_TAKE()) {
        g_state.tc_cielo_err = err_cielo;
        if (!err_cielo) g_state.temp_cielo = (double)t_cielo_raw;

        if (g_state.sensor_mode == SensorMode::DUAL) {
          g_state.tc_base_err = err_base;
          if (!err_base) g_state.temp_base = (double)t_base_raw;
        } else {
          g_state.tc_base_err = false;
          g_state.temp_base   = g_state.temp_cielo;
        }

        pid_base.setTunings (g_state.kp_base,  g_state.ki_base,  g_state.kd_base);
        pid_cielo.setTunings(g_state.kp_cielo, g_state.ki_cielo, g_state.kd_cielo);

        if (g_state.sensor_mode == SensorMode::SINGLE) {
          if (g_state.base_enabled && !g_state.tc_cielo_err)
            pid_cielo.compute();
          g_state.pid_out_base = g_state.pid_out_cielo;
        } else {
          if (g_state.base_enabled  && !g_state.tc_base_err)  pid_base.compute();
          if (g_state.cielo_enabled && !g_state.tc_cielo_err) pid_cielo.compute();
        }
        MUTEX_GIVE();
      }
    }

    // ── 4. Relay ──
    if (!autotune_is_running()) {
      if (g_state.sensor_mode == SensorMode::SINGLE) {
        double t   = g_state.temp_cielo;
        double sp  = g_state.set_cielo;
        bool   en  = g_state.base_enabled;
        bool   ph  = en && (t < sp - PREHEAT_MARGIN_DEG);
        g_state.preheat_base  = ph;
        g_state.preheat_cielo = ph;

        if (ph) {
          if (!g_state.relay_base)  { g_state.relay_base  = true; RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  true); }
          if (!g_state.relay_cielo) { g_state.relay_cielo = true; RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, true); }
          if (MUTEX_TAKE()) { g_state.pid_out_cielo = 100.0; g_state.pid_out_base = 100.0; MUTEX_GIVE(); }
        } else {
          pid_cielo.updateRelay(en, RELAY_BASE,  RELAY_BASE_INV,  g_state.relay_base,  g_state.pct_base);
          pid_cielo.updateRelay(en, RELAY_CIELO, RELAY_CIELO_INV, g_state.relay_cielo, g_state.pct_cielo);
        }
      } else {
        // BASE
        {
          double t  = g_state.temp_base;
          double sp = g_state.set_base;
          bool   en = g_state.base_enabled;
          bool   ph = en && !g_state.tc_base_err && (t < sp - PREHEAT_MARGIN_DEG);
          g_state.preheat_base = ph;
          if (!en) {
            if (g_state.relay_base) { g_state.relay_base = false; RELAY_WRITE(RELAY_BASE, RELAY_BASE_INV, false); }
          } else if (ph) {
            if (!g_state.relay_base) { g_state.relay_base = true; RELAY_WRITE(RELAY_BASE, RELAY_BASE_INV, true); }
            if (MUTEX_TAKE()) { g_state.pid_out_base = 100.0; MUTEX_GIVE(); }
          } else {
            pid_base.updateRelay(en, RELAY_BASE, RELAY_BASE_INV, g_state.relay_base, g_state.pct_base);
          }
        }
        // CIELO
        {
          double t  = g_state.temp_cielo;
          double sp = g_state.set_cielo;
          bool   en = g_state.cielo_enabled;
          bool   ph = en && !g_state.tc_cielo_err && (t < sp - PREHEAT_MARGIN_DEG);
          g_state.preheat_cielo = ph;
          if (!en) {
            if (g_state.relay_cielo) { g_state.relay_cielo = false; RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false); }
          } else if (ph) {
            if (!g_state.relay_cielo) { g_state.relay_cielo = true; RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, true); }
            if (MUTEX_TAKE()) { g_state.pid_out_cielo = 100.0; MUTEX_GIVE(); }
          } else {
            pid_cielo.updateRelay(en, RELAY_CIELO, RELAY_CIELO_INV, g_state.relay_cielo, g_state.pct_cielo);
          }
        }
      }
    }

    // ── 5. Ventola ──
    bool any_resistance_on = g_state.relay_base || g_state.relay_cielo
                             || g_state.base_enabled || g_state.cielo_enabled;
    bool fan_needed = any_resistance_on || (!err_cielo && t_cielo_raw > FAN_OFF_TEMP);
    if (fan_needed != g_state.fan_on) {
      g_state.fan_on = fan_needed;
      RELAY_WRITE(RELAY_FAN, RELAY_FAN_INV, fan_needed);
      Serial.printf("[PID] Ventola %s (T=%.1f)\n", fan_needed ? "ON" : "OFF", t_cielo_raw);
    }

    // ── 6. Luce ──
#if AUTO_LUCE_WITH_HEAT
    if (any_resistance_on && !g_state.luce_on) {
      g_state.luce_on = true;
    }
#endif
    RELAY_WRITE(RELAY_LUCE, RELAY_LUCE_INV, g_state.luce_on);

    // ── 7. NVS ──
    if (g_state.nvs_dirty) {
      if (now - last_nvs_ms >= 3000) nvs_save_from_state();
    } else {
      last_nvs_ms = now;
    }

    // ── 8. Heartbeat ──
    g_pid_heartbeat++;

    // ── 9. Grafico ──
    static uint32_t last_graph_ms = 0;
    if (now - last_graph_ms >= (uint32_t)(GRAPH_SAMPLE_S * 1000)) {
      last_graph_ms = now;
      if (MUTEX_TAKE()) {
        g_graph.base [g_graph.head] = (float)g_state.temp_base;
        g_graph.cielo[g_graph.head] = (float)g_state.temp_cielo;
        g_graph.head = (g_graph.head + 1) % GRAPH_BUF_SIZE;
        if (g_graph.count < GRAPH_BUF_SIZE) g_graph.count++;
        MUTEX_GIVE();
      }
    }

    Serial.printf("[C1 %5.1fs] %s B:%5.1f/%.0f %3.0f%%%c C:%5.1f/%.0f %3.0f%%%c Fan:%c\n",
      now / 1000.0,
      g_state.sensor_mode == SensorMode::SINGLE ? "SGL" : "DUA",
      g_state.temp_base,  g_state.set_base,  g_state.pid_out_base,
      g_state.relay_base  ? '*' : '.',
      g_state.temp_cielo, g_state.set_cielo, g_state.pid_out_cielo,
      g_state.relay_cielo ? '*' : '.',
      g_state.fan_on      ? 'V' : '-');

    vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
  }
}

// ================================================================
//  TASK_LVGL — invariato (risoluzione aggiornata via DISP_W/H)
// ================================================================
static void Task_LVGL(void* param) {
  Serial.printf("[Core %d] Task_LVGL avviato\n", xPortGetCoreID());
  uint32_t last_ui_ms = 0;

  for (;;) {
    lv_tick_inc(1);
    lv_timer_handler();

    uint32_t now = millis();
    if (now - last_ui_ms >= 100) {
      last_ui_ms = now;
      Screen scr;
      if (MUTEX_TAKE()) {
        scr = g_state.active_screen;
        MUTEX_GIVE();
        switch (scr) {
          case Screen::MAIN:      ui_refresh(&g_state);       break;
          case Screen::TEMP:      ui_refresh_temp(&g_state);  break;
          case Screen::PID_BASE:
          case Screen::PID_CIELO: ui_refresh_pid(&g_state);   break;
          case Screen::GRAPH:     ui_refresh_graph(&g_state); break;
          case Screen::WIFI_SCAN:
              ui_wifi_update_list();
              ui_wifi_update_status();
              break;
          case Screen::OTA:
              ui_ota_update_progress();
              break;
          case Screen::WIFI_PWD:
              break;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.printf("\n=== FORNO PIZZA v15.0 — ESP32-S3 | ST7701S 480x272 | GT911 ===\n");
  Serial.printf("setup() su Core %d\n", xPortGetCoreID());

  // ---- Relay OFF come prima cosa assoluta ----
  RELAY_INIT(RELAY_BASE,  RELAY_BASE_INV);
  RELAY_INIT(RELAY_CIELO, RELAY_CIELO_INV);
  RELAY_INIT(RELAY_LUCE,  RELAY_LUCE_INV);
  RELAY_INIT(RELAY_FAN,   RELAY_FAN_INV);

  // ---- Display (LovyanGFX gestisce anche backlight e touch init) ----
  lcd.init();
  lcd.setRotation(1);        // landscape
  lcd.fillScreen(TFT_BLACK);
  lcd.setBrightness(255);    // backlight al massimo

  // ---- Mutex ----
  g_mutex = xSemaphoreCreateMutex();
  if (!g_mutex) {
    Serial.println("ERRORE FATALE: mutex non creato!");
    while (true) vTaskDelay(1000);
  }

  nvs_load_to_state();
  Serial.printf("NVS: mode=%s set=%.0f/%.0f split=%d/%d\n",
    g_state.sensor_mode == SensorMode::SINGLE ? "SINGLE" : "DUAL",
    g_state.set_base, g_state.set_cielo,
    g_state.pct_base, g_state.pct_cielo);

  // ---- LVGL ----
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, lb1, lb2, DISP_W * 25);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = DISP_W;
  disp_drv.ver_res  = DISP_H;
  disp_drv.flush_cb = lvgl_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvgl_touch;
  lv_indev_drv_register(&indev_drv);

  // ---- PID ----
  pid_base.begin();
  pid_base.setTunings(g_state.kp_base, g_state.ki_base, g_state.kd_base);
  pid_cielo.begin();
  pid_cielo.setTunings(g_state.kp_cielo, g_state.ki_cielo, g_state.kd_cielo);

  // ---- Prima lettura sensori ----
  delay(300);
  float t2 = tc_cielo.readCelsius();
  g_state.tc_cielo_err = isnan(t2) || t2 <= 0.0f;
  if (!g_state.tc_cielo_err) g_state.temp_cielo = (double)t2;
  if (g_state.sensor_mode == SensorMode::SINGLE) {
    g_state.tc_base_err = false;
    g_state.temp_base   = g_state.temp_cielo;
  } else {
    float t1 = tc_base.readCelsius();
    g_state.tc_base_err = isnan(t1) || t1 <= 0.0f;
    if (!g_state.tc_base_err) g_state.temp_base = (double)t1;
  }

  ui_init();
  ui_refresh(&g_state);

  // ---- WiFi + MQTT ----
  wifi_mqtt_init();

  // ---- OTA ----
  ota_manager_init();
  web_ota_init();

  // ---- Task ----
  xTaskCreatePinnedToCore(Task_Watchdog, "Task_WDG",
    TASK_WDG_STACK, nullptr, TASK_WDG_PRIO, nullptr, TASK_WDG_CORE);

  g_pid_heartbeat = 1;

  xTaskCreatePinnedToCore(Task_PID,  "Task_PID",
    TASK_PID_STACK, nullptr, TASK_PID_PRIO, nullptr, TASK_PID_CORE);

  xTaskCreatePinnedToCore(Task_LVGL, "Task_LVGL",
    TASK_LVGL_STACK, nullptr, TASK_LVGL_PRIO, nullptr, TASK_LVGL_CORE);

  xTaskCreatePinnedToCore(Task_WiFi, "Task_WiFi",
    TASK_WIFI_STACK, nullptr, TASK_WIFI_PRIO, nullptr, TASK_WIFI_CORE);

  Serial.printf("Safety: TEMP_MAX=%.0f FAN_OFF=%.0f WDG=%dms\n",
    TEMP_MAX_SAFE, FAN_OFF_TEMP, WDG_TIMEOUT_MS);
  Serial.println("Tutti i task avviati.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
