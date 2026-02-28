/**
 * FornoPizza_S3.ino — Forno Pizza Controller
 * Porting su JC4827W543C (ST7701S SPI 480×272 + CST820 touch capacitivo)
 *
 * HARDWARE:
 *   MCU:     ESP32-S3-WROOM-1-N4R2 (4MB flash, 2MB PSRAM)
 *   Display: JC4827W543C  480×272  ST7701S  SPI
 *   Touch:   CST820  I2C  SDA=8 SCL=9 INT=3 RST=38  addr=0x15
 *   TC:      MAX6675 x2  SPI software  SCK=11 MISO=12 CS_B=13 CS_C=14
 *   Relè:    Base=15  Cielo=16  Luce=17  Fan=10
 *
 * LIBRERIE (Library Manager):
 *   - LovyanGFX        >= 1.1.12
 *   - lvgl             >= 8.3.0    (lv_conf.h nella cartella Arduino/libraries)
 *   - max6675           (Adafruit MAX6675)
 *   - PID_v1            (Brett Beauregard)
 *   - Preferences       (NVS, inclusa in ESP32 Arduino core)
 *
 * ARCHITETTURA DUAL CORE:
 *   Core 0 — Task_LVGL  (display, touch, UI refresh)
 *   Core 1 — Task_PID   (sensori, PID, relè)
 *             Task_Watchdog (sorveglianza Task_PID)
 *
 * SICUREZZA:
 *   - Shutdown automatico se TC errore, temp > TEMP_MAX_SAFE
 *   - Thermal runaway detection (salita rapida, discesa prolungata)
 *   - Watchdog software su Task_PID
 *
 * NOTE PORTING da versione ESP32:
 *   - TFT_eSPI        → LovyanGFX (display_driver.h)
 *   - XPT2046         → FT6x36    (touch_driver.h)
 *   - TFT_BL_PIN=21   → LCD_BL=1  (hardware.h)
 *   - RELAY_FAN su GPIO10 (quarto relè ventola)
 *   - WiFi/MQTT disabilitato (da aggiungere nella fase successiva)
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <max6675.h>
#include <PID_v1.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "hardware.h"
#include "display_driver.h"
#include "touch_driver.h"
#include "pid_ctrl.h"
#include "nvs_storage.h"
#include "ui.h"
#include "ui_wifi.h"
#include "autotune.h"
#include "wifi_mqtt.h"
#include "ota_manager.h"
#include "web_ota.h"

// ================================================================
//  ISTANZE HARDWARE
// ================================================================
LGFX                gfx;
// Touch CST820 gestito interamente in touch_driver.h via Wire (nessuna istanza)
MAX6675             tc_base (TC_SCK, TC_CS_BASE,  TC_MISO);
MAX6675             tc_cielo(TC_SCK, TC_CS_CIELO, TC_MISO);
NVSStorage          nvs;

// ================================================================
//  STATO GLOBALE
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
//  RING BUFFER GRAFICO
// ================================================================
GraphBuffer g_graph = {};

// ================================================================
//  PID
// ================================================================
PIDController pid_base (&g_state.temp_base,  &g_state.pid_out_base,
                         &g_state.set_base,
                         g_state.kp_base, g_state.ki_base, g_state.kd_base);
PIDController pid_cielo(&g_state.temp_cielo, &g_state.pid_out_cielo,
                         &g_state.set_cielo,
                         g_state.kp_cielo, g_state.ki_cielo, g_state.kd_cielo);

// ================================================================
//  MUTEX
// ================================================================
SemaphoreHandle_t g_mutex = nullptr;

// ================================================================
//  WATCHDOG
// ================================================================
volatile uint32_t g_pid_heartbeat    = 0;
volatile bool     g_emergency_shutdown = false;

// ================================================================
//  EMERGENCY SHUTDOWN — IRAM_ATTR (sicuro anche in caso di crash heap)
// ================================================================
void IRAM_ATTR emergency_shutdown(SafetyReason reason) {
  // Relay OFF immediatamente
  RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
  RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
  RELAY_WRITE(RELAY_LUCE,  RELAY_LUCE_INV,  false);
  // Ventola: lasciala ON in emergenza per raffreddare, si spegne dopo FAN_OFF_TEMP
  // RELAY_WRITE(RELAY_FAN, RELAY_FAN_INV, false);

  g_emergency_shutdown = true;

  if (MUTEX_TAKE()) {
    g_state.safety_shutdown = true;
    g_state.safety_reason   = reason;
    g_state.base_enabled    = false;
    g_state.cielo_enabled   = false;
    g_state.relay_base      = false;
    g_state.relay_cielo     = false;
    MUTEX_GIVE();
  }
  Serial.printf("[SAFETY] SHUTDOWN! Reason: %d\n", (int)reason);
}

// ================================================================
//  TASK_WATCHDOG — Core 1 (priorità massima)
// ================================================================
static void Task_Watchdog(void* param) {
  uint32_t last_beat   = 0;
  uint32_t last_beat_time = millis();

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(WDG_CHECK_MS));
    uint32_t now  = millis();
    uint32_t beat = g_pid_heartbeat;

    if (beat != last_beat) {
      last_beat      = beat;
      last_beat_time = now;
    } else if (!g_emergency_shutdown &&
               (g_state.base_enabled || g_state.cielo_enabled) &&
               (now - last_beat_time > WDG_TIMEOUT_MS)) {
      Serial.printf("[WDG] Task_PID heartbeat fermo da %lums\n", now - last_beat_time);
      emergency_shutdown(SafetyReason::WDG_TIMEOUT);
    }
  }
}

// ================================================================
//  TASK_PID — Core 1
// ================================================================
static void Task_PID(void* param) {
  Serial.printf("[Core %d] Task_PID avviato\n", xPortGetCoreID());

  unsigned long win_base_single  = 0;
  unsigned long win_cielo_single = 0;
  uint32_t      last_nvs_ms      = millis();
  uint32_t      last_graph_ms    = millis();

  // Runaway DOWN
  float    rd_peak_temp     = -1.0f;
  float    rd_valley_temp   = -1.0f;
  uint32_t rd_drop_start_ms = 0;
  bool     rd_drop_active   = false;

  // Runaway UP
  float    ru_start_temp = -1.0f;
  uint32_t ru_start_ms   = 0;

  for (;;) {
    uint32_t now = millis();

    // Shutdown: relay OFF, attendi
    if (g_emergency_shutdown) {
      RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
      RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
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
      // Soglia assoluta
      if (t_cielo_raw > TEMP_MAX_SAFE) {
        emergency_shutdown(SafetyReason::OVER_TEMP);
        vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
        continue;
      }

#if RUNAWAY_UP_ENABLED
      // Runaway UP
      if (g_state.base_enabled || g_state.cielo_enabled) {
        if (ru_start_temp < 0.0f) { ru_start_temp = t_cielo_raw; ru_start_ms = now; }
        if ((now - ru_start_ms) >= RUNAWAY_RISE_MS) {
          if ((t_cielo_raw - ru_start_temp) > RUNAWAY_RISE_DEG)
            emergency_shutdown(SafetyReason::RUNAWAY_UP);
          ru_start_temp = t_cielo_raw; ru_start_ms = now;
        }
      } else { ru_start_temp = -1.0f; }
#endif

#if RUNAWAY_DOWN_ENABLED
      // Runaway DOWN
      if (g_state.relay_cielo || g_state.relay_base) {
        if (rd_peak_temp < t_cielo_raw) {
          rd_peak_temp   = t_cielo_raw;
          rd_drop_active = false;
        }
        float drop = rd_peak_temp - t_cielo_raw;
        if (drop >= RUNAWAY_MIN_DROP) {
          if (!rd_drop_active) { rd_drop_active = true; rd_drop_start_ms = now; }
          if ((now - rd_drop_start_ms) > RUNAWAY_DOWN_MS)
            emergency_shutdown(SafetyReason::RUNAWAY_DOWN);
        } else if (rd_drop_active && drop < (RUNAWAY_MIN_DROP - RUNAWAY_RECOVERY_DEG)) {
          rd_drop_active = false;
        }
      } else { rd_peak_temp = -1.0f; rd_drop_active = false; }
#endif
    }

    // ── 3. Aggiorna stato + PID ──
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

      if (!autotune_is_running()) {
        if (g_state.sensor_mode == SensorMode::SINGLE) {
          if (g_state.base_enabled && !g_state.tc_cielo_err) pid_cielo.compute();
          g_state.pid_out_base = g_state.pid_out_cielo;
        } else {
          if (g_state.base_enabled  && !g_state.tc_base_err)  pid_base.compute();
          if (g_state.cielo_enabled && !g_state.tc_cielo_err) pid_cielo.compute();
        }
      }
      MUTEX_GIVE();
    }

    // ── 4. Relay ──
    if (!autotune_is_running()) {
      if (g_state.sensor_mode == SensorMode::SINGLE) {
        double t  = g_state.temp_cielo;
        double sp = g_state.set_cielo;
        bool   en = g_state.base_enabled;
        bool   ph = en && (t < sp - PREHEAT_MARGIN_DEG);
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
        // DUAL
        { double t=g_state.temp_base, sp=g_state.set_base; bool en=g_state.base_enabled;
          bool ph=en&&!g_state.tc_base_err&&(t<sp-PREHEAT_MARGIN_DEG);
          g_state.preheat_base=ph;
          if (!en) { if(g_state.relay_base){g_state.relay_base=false;RELAY_WRITE(RELAY_BASE,RELAY_BASE_INV,false);}}
          else if(ph) { if(!g_state.relay_base){g_state.relay_base=true;RELAY_WRITE(RELAY_BASE,RELAY_BASE_INV,true);} if(MUTEX_TAKE()){g_state.pid_out_base=100.0;MUTEX_GIVE();}}
          else pid_base.updateRelay(en,RELAY_BASE,RELAY_BASE_INV,g_state.relay_base,g_state.pct_base); }
        { double t=g_state.temp_cielo, sp=g_state.set_cielo; bool en=g_state.cielo_enabled;
          bool ph=en&&!g_state.tc_cielo_err&&(t<sp-PREHEAT_MARGIN_DEG);
          g_state.preheat_cielo=ph;
          if (!en) { if(g_state.relay_cielo){g_state.relay_cielo=false;RELAY_WRITE(RELAY_CIELO,RELAY_CIELO_INV,false);}}
          else if(ph) { if(!g_state.relay_cielo){g_state.relay_cielo=true;RELAY_WRITE(RELAY_CIELO,RELAY_CIELO_INV,true);} if(MUTEX_TAKE()){g_state.pid_out_cielo=100.0;MUTEX_GIVE();}}
          else pid_cielo.updateRelay(en,RELAY_CIELO,RELAY_CIELO_INV,g_state.relay_cielo,g_state.pct_cielo); }
      }
    }

    // ── 5. Luce ──
    RELAY_WRITE(RELAY_LUCE, RELAY_LUCE_INV, g_state.luce_on);

    // ── 5b. Ventola ──
    // Auto: ON se almeno una resistenza è attiva OPPURE temp > FAN_OFF_TEMP
    // Manuale override: g_state.fan_on può essere forzato dalla UI
    {
      bool any_heater_on = g_state.relay_base || g_state.relay_cielo;
      bool temp_high     = (g_state.temp_base  > FAN_OFF_TEMP) ||
                           (g_state.temp_cielo > FAN_OFF_TEMP);
      bool fan_auto = any_heater_on || temp_high;
      if (MUTEX_TAKE()) {
        g_state.fan_on = fan_auto;
        MUTEX_GIVE();
      }
      RELAY_WRITE(RELAY_FAN, RELAY_FAN_INV, fan_auto);
    }

    // ── 6. Grafico ──
    if (now - last_graph_ms >= (GRAPH_SAMPLE_S * 1000UL)) {
      last_graph_ms = now;
      if (MUTEX_TAKE()) {
        g_graph.base [g_graph.head] = (float)g_state.temp_base;
        g_graph.cielo[g_graph.head] = (float)g_state.temp_cielo;
        g_graph.head = (g_graph.head + 1) % GRAPH_BUF_SIZE;
        if (g_graph.count < GRAPH_BUF_SIZE) g_graph.count++;
        MUTEX_GIVE();
      }
    }

    // ── 7. NVS save ──
    if (now - last_nvs_ms >= 30000UL) {
      last_nvs_ms = now;
      if (g_state.nvs_dirty) { nvs.save(g_state); g_state.nvs_dirty = false; }
    }

    // ── 8. Log seriale ──
    Serial.printf("[C1 %5.1fs] %s B:%5.1f/%.0f %3.0f%%%c C:%5.1f/%.0f %3.0f%%%c\n",
      now / 1000.0,
      g_state.sensor_mode == SensorMode::SINGLE ? "SGL" : "DUA",
      g_state.temp_base,  g_state.set_base,  g_state.pid_out_base,
      g_state.relay_base  ? '*' : '.',
      g_state.temp_cielo, g_state.set_cielo, g_state.pid_out_cielo,
      g_state.relay_cielo ? '*' : '.');

    g_pid_heartbeat++;
    vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
  }
}

// ================================================================
//  TASK_LVGL — Core 0
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
          default: break;
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
  Serial.println("\n=== FORNO PIZZA S3 — ESP32-S3 | JC4827W543C 480x272 ===");
  Serial.printf("setup() su Core %d\n", xPortGetCoreID());

  // ── Relay OFF come prima cosa assoluta ──
  RELAY_INIT(RELAY_BASE,  RELAY_BASE_INV);
  RELAY_INIT(RELAY_CIELO, RELAY_CIELO_INV);
  RELAY_INIT(RELAY_LUCE,  RELAY_LUCE_INV);
  RELAY_INIT(RELAY_FAN,   RELAY_FAN_INV);

  // ── Display + Touch ──
  display_init();
  touch_init();

  // ── Mutex ──
  g_mutex = xSemaphoreCreateMutex();
  if (!g_mutex) {
    Serial.println("ERRORE FATALE: mutex non creato!");
    while (true) vTaskDelay(1000);
  }

  // ── NVS ──
  nvs.load(g_state);
  Serial.printf("NVS: mode=%s set=%.0f/%.0f\n",
    g_state.sensor_mode == SensorMode::SINGLE ? "SINGLE" : "DUAL",
    g_state.set_base, g_state.set_cielo);

  // ── PID init ──
  pid_base.begin();
  pid_cielo.begin();

  // ── UI init ──
  ui_init();
  ui_show_screen(Screen::MAIN);

  // ── WiFi + MQTT ──
  wifi_mqtt_init();

  // ── OTA ──
  ota_manager_init();
  web_ota_init();

  // ── Task ──
  xTaskCreatePinnedToCore(Task_Watchdog, "WDG",     2048, nullptr, 3, nullptr, 1);
  g_pid_heartbeat = 1;
  xTaskCreatePinnedToCore(Task_PID,     "PID",     4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(Task_LVGL,    "LVGL",    8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(Task_WiFi,    "WiFi",    8192, nullptr, 1, nullptr, 0);

  Serial.println("=== SETUP COMPLETATO ===");
}

// ================================================================
//  LOOP — non usato (tutto nei task FreeRTOS)
// ================================================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(10000));
}
