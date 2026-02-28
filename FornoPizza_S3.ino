/**
 * FornoPizza_S3.ino — Forno Pizza Controller
 * Hardware: ESP32-S3-WROOM-1-N4R8 | JC4827W543C 480x272 ST7701S
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
#include "splash_screen.h"
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
LGFX    gfx;
MAX6675  tc_base (TC_SCK, TC_CS_BASE,  TC_MISO);
MAX6675  tc_cielo(TC_SCK, TC_CS_CIELO, TC_MISO);
NVSStorage nvs;

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
  .kp_base  = 3.0,  .ki_base  = 0.02, .kd_base  = 2.0,
  .kp_cielo = 2.5,  .ki_cielo = 0.03, .kd_cielo = 1.5,
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

GraphBuffer g_graph = {};

// ================================================================
//  PID — le istanze usano puntatori a g_state, che è già inizializzato
// ================================================================
PIDController pid_base (&g_state.temp_base,  &g_state.pid_out_base,
                         &g_state.set_base,
                         g_state.kp_base, g_state.ki_base, g_state.kd_base);
PIDController pid_cielo(&g_state.temp_cielo, &g_state.pid_out_cielo,
                         &g_state.set_cielo,
                         g_state.kp_cielo, g_state.ki_cielo, g_state.kd_cielo);

// ================================================================
//  MUTEX / WATCHDOG / EMERGENCY
// ================================================================
SemaphoreHandle_t g_mutex            = nullptr;
volatile uint32_t g_pid_heartbeat    = 0;
volatile bool     g_emergency_shutdown = false;

static void emergency_shutdown(SafetyReason reason) {
  g_emergency_shutdown = true;
  RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
  RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
  RELAY_WRITE(RELAY_FAN,   RELAY_FAN_INV,   true);
  if (MUTEX_TAKE()) {
    g_state.safety_shutdown = true;
    g_state.safety_reason   = reason;
    g_state.base_enabled    = false;
    g_state.cielo_enabled   = false;
    MUTEX_GIVE();
  }
  Serial.printf("[SAFETY] EMERGENCY SHUTDOWN! Reason: %d\n", (int)reason);
}

// ================================================================
//  HELPER NVS — converte AppState <-> NVSData
//  NVSStorage lavora su NVSData (struct più piccola, solo i campi
//  persistiti). Qui copiamo i valori rilevanti in entrambe le direzioni.
// ================================================================
static void nvs_load_into_state(NVSStorage& storage, AppState& s) {
  NVSData d;
  storage.load(d);
  s.set_base    = d.set_base;
  s.set_cielo   = d.set_cielo;
  s.kp_base     = d.kp_base;
  s.ki_base     = d.ki_base;
  s.kd_base     = d.kd_base;
  s.kp_cielo    = d.kp_cielo;
  s.ki_cielo    = d.ki_cielo;
  s.kd_cielo    = d.kd_cielo;
  s.sensor_mode = d.single_mode ? SensorMode::SINGLE : SensorMode::DUAL;
  s.pct_base    = d.pct_base;
  s.pct_cielo   = d.pct_cielo;
}

static void nvs_save_from_state(NVSStorage& storage, const AppState& s) {
  NVSData d;
  d.set_base    = (float)s.set_base;
  d.set_cielo   = (float)s.set_cielo;
  d.kp_base     = (float)s.kp_base;
  d.ki_base     = (float)s.ki_base;
  d.kd_base     = (float)s.kd_base;
  d.kp_cielo    = (float)s.kp_cielo;
  d.ki_cielo    = (float)s.ki_cielo;
  d.kd_cielo    = (float)s.kd_cielo;
  d.single_mode = (s.sensor_mode == SensorMode::SINGLE) ? 1 : 0;
  d.pct_base    = s.pct_base;
  d.pct_cielo   = s.pct_cielo;
  storage.save(d);
}

// ================================================================
//  TASK_WATCHDOG — Core 1
// ================================================================
static void Task_Watchdog(void* param) {
  uint32_t last_beat      = 0;
  uint32_t last_beat_time = millis();
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(WDG_CHECK_MS));
    uint32_t now  = millis();
    uint32_t beat = g_pid_heartbeat;
    if (beat != last_beat) {
      last_beat = beat; last_beat_time = now;
    } else if (!g_emergency_shutdown &&
               (g_state.base_enabled || g_state.cielo_enabled) &&
               (now - last_beat_time > WDG_TIMEOUT_MS)) {
      emergency_shutdown(SafetyReason::WDG_TIMEOUT);
    }
  }
}

// ================================================================
//  TASK_PID — Core 1
// ================================================================
static void Task_PID(void* param) {
  Serial.printf("[Core %d] Task_PID avviato\n", xPortGetCoreID());
  uint32_t last_nvs_ms   = millis();
  uint32_t last_graph_ms = millis();

  for (;;) {
    uint32_t now = millis();

    if (g_emergency_shutdown) {
      RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
      RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
      g_pid_heartbeat++;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // ── Lettura sensori ──
    float t_base_raw  = tc_base.readCelsius();
    float t_cielo_raw = tc_cielo.readCelsius();
    bool  err_base    = isnan(t_base_raw)  || t_base_raw  <= 0.0f;
    bool  err_cielo   = isnan(t_cielo_raw) || t_cielo_raw <= 0.0f;

    if (MUTEX_TAKE()) {
      g_state.tc_base_err  = err_base;
      g_state.tc_cielo_err = err_cielo;
      if (!err_base)  g_state.temp_base  = t_base_raw;
      if (!err_cielo) g_state.temp_cielo = t_cielo_raw;
      MUTEX_GIVE();
    }

    if ((err_base || err_cielo) && (g_state.base_enabled || g_state.cielo_enabled)) {
      emergency_shutdown(SafetyReason::TC_ERROR);
    }

    // ── Aggiornamento PID ──
    //pid_ctrl_update(now);

    // ── Autotune ──
    autotune_run(t_cielo_raw, now);

    // ── NVS save periodico ──
    if (now - last_nvs_ms >= 30000UL) {
      last_nvs_ms = now;
      if (g_state.nvs_dirty) {
        nvs_save_from_state(nvs, g_state);
        g_state.nvs_dirty = false;
      }
    }

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
      if (MUTEX_TAKE()) {
        Screen scr = g_state.active_screen;
        MUTEX_GIVE();
        switch (scr) {
          case Screen::MAIN:      ui_refresh(&g_state);       break;
          case Screen::TEMP:      ui_refresh_temp(&g_state);  break;
          case Screen::PID_BASE:
          case Screen::PID_CIELO: ui_refresh_pid(&g_state);   break;
          case Screen::GRAPH:     ui_refresh_graph(&g_state); break;
          case Screen::WIFI_SCAN: ui_wifi_update_list(); ui_wifi_update_status(); break;
          case Screen::OTA:       ui_ota_update_progress(); break;
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
  Serial.printf("Free PSRAM: %u byte\n", ESP.getFreePsram());

  // 1. Relay OFF
  RELAY_INIT(RELAY_BASE,  RELAY_BASE_INV);
  RELAY_INIT(RELAY_CIELO, RELAY_CIELO_INV);
  RELAY_INIT(RELAY_LUCE,  RELAY_LUCE_INV);
  RELAY_INIT(RELAY_FAN,   RELAY_FAN_INV);

  // 2. Display + Touch
  display_init();
  touch_init();

  // 3. Splash
  splash_init();
  splash_set_progress(5, "Display OK...");

  // 4. Mutex
  g_mutex = xSemaphoreCreateMutex();
  if (!g_mutex) {
    Serial.println("ERRORE FATALE: mutex!");
    while (true) vTaskDelay(1000);
  }
  splash_set_progress(15, "Sistema OK...");

  // 5. NVS — carica in g_state tramite helper
  nvs_load_into_state(nvs, g_state);
  Serial.printf("NVS: mode=%s set=%.0f/%.0f\n",
    g_state.sensor_mode == SensorMode::SINGLE ? "SINGLE" : "DUAL",
    g_state.set_base, g_state.set_cielo);
  splash_set_progress(30, "Impostazioni caricate...");

  // 6. PID — aggiorna tuning dai valori NVS appena caricati
  pid_base.begin();
  pid_cielo.begin();
  pid_base.setTunings(g_state.kp_base,  g_state.ki_base,  g_state.kd_base);
  pid_cielo.setTunings(g_state.kp_cielo, g_state.ki_cielo, g_state.kd_cielo);
  splash_set_progress(45, "PID inizializzato...");

  // 7. UI
  splash_set_progress(55, "Costruzione interfaccia...");
  ui_init();
  splash_set_progress(70, "Interfaccia pronta...");

  // 8. WiFi + MQTT
  splash_set_progress(78, "Avvio WiFi...");
  wifi_mqtt_init();
  splash_set_progress(88, "WiFi OK...");

  // 9. OTA
  splash_set_progress(93, "Avvio OTA...");
  ota_manager_init();
  web_ota_init();
  splash_set_progress(98, "Tutto pronto!");

  // 10. Task FreeRTOS
  xTaskCreatePinnedToCore(Task_Watchdog, "WDG",  2048, nullptr, 3, nullptr, 1);
  g_pid_heartbeat = 1;
  xTaskCreatePinnedToCore(Task_PID,     "PID",  4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(Task_LVGL,    "LVGL", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(Task_WiFi,    "WiFi", 8192, nullptr, 1, nullptr, 0);

  // 11. Fade-out splash → MAIN
  splash_finish(ui_ScreenMain);

  Serial.println("=== SETUP COMPLETATO ===");
  Serial.printf("Free heap: %u  Free PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

// ================================================================
//  LOOP — tutto nei task FreeRTOS
// ================================================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(10000));
}
