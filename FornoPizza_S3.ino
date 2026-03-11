/**
 * ================================================================
 *  FORNO PIZZA CONTROLLER v17.0 — ESP32-S3
 *  Board  : ESP32-S3-WROOM-1-N16R8
 *  Display: 4.3" ST7701A 480×272 (SPI 4-wire)
 *  Touch  : CST820 capacitivo (I2C)
 *
 *  Per abilitare/disabilitare task, feature e log:
 *    → modificare debug_config.h
 *
 *  FIX v17 (da ESP32_CYD_LVGL_BugFix_Guide):
 *  ─────────────────────────────────────────
 *  [1] Oggetti hardware → puntatori (MAX6675, NVSStorage, PIDController)
 *  [2] Doppia init LVGL rimossa — display_init() gestisce tutto
 *  [3] TASK_LVGL_STACK: 8192 → 16384
 *  [4] MUTEX_TIMEOUT_MS: 10 → 50
 *  [5] Watchdog: vTaskDelay(2000) + g_pid_heartbeat=1 prima della creazione
 *  [6] g_pid_heartbeat++ PRIMA di readCelsius()
 *  [7] Splash: lv_scr_load_anim → lv_scr_load, LV_ANIM_OFF
 *  [8] lv_conf.h: LV_ATTRIBUTE_FAST_MEM vuoto, LV_MEM_POOL_* commentate,
 *                 LV_TXT_COLOR_CMD "#", LV_MEM_CUSTOM=1 PSRAM
 *
 *  MEMORIA:
 *  ────────
 *  Framebuffer LVGL:  double full FB 480×272 in PSRAM OPI (510 KB)
 *  Oggetti UI LVGL:   heap PSRAM via LV_MEM_CUSTOM=1
 *  GraphBuffer:       array base/cielo in PSRAM (~2.8 KB)
 *  WiFi/PID/WDG task: stack in SRAM interna (richiesto da ISR/WiFi)
 *  LVGL task:         stack in SRAM interna (16384 byte)
 * ================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <max6675.h>
#include <PID_v1.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ── PRIMO include: tutte le #define di debug/task/log ───────────
#include "debug_config.h"

// ── Display + LVGL ──────────────────────────────────────────────
#include "display_driver.h"

// ── Core progetto ───────────────────────────────────────────────
#include "hardware.h"
#include "ui.h"
#include "pid_ctrl.h"
#include "nvs_storage.h"
#include "splash_screen.h"

#if TASK_WIFI_ENABLE
  #include "wifi_mqtt.h"
  #include "ui_wifi.h"
#endif

#if FEATURE_AUTOTUNE
  #include "autotune.h"
#endif

#if FEATURE_OTA && TASK_WIFI_ENABLE
  #include "ota_manager.h"
  #include "web_ota.h"
#endif

// ================================================================
//  OGGETTI HARDWARE — puntatori (FIX: no costruttori globali)
// ================================================================
static MAX6675*       tc_base  = nullptr;
static MAX6675*       tc_cielo = nullptr;
static NVSStorage*    nvs      = nullptr;
static PIDController* pid_base  = nullptr;
static PIDController* pid_cielo = nullptr;

// ================================================================
//  SINCRONIZZAZIONE
// ================================================================
SemaphoreHandle_t    g_mutex              = nullptr;
volatile uint32_t    g_pid_heartbeat      = 0;
volatile bool        g_emergency_shutdown = false;

// ================================================================
//  STATO APPLICAZIONE (SRAM interna — accesso real-time)
// ================================================================
AppState    g_state = {};
extern GraphBuffer g_graph;

// ================================================================
//  EMERGENCY SHUTDOWN
// ================================================================
void IRAM_ATTR emergency_shutdown(SafetyReason reason) {
  RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
  RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);

  g_emergency_shutdown    = true;
  g_state.relay_base      = false;
  g_state.relay_cielo     = false;
  g_state.base_enabled    = false;
  g_state.cielo_enabled   = false;
  g_state.safety_shutdown = true;
  g_state.safety_reason   = reason;

  const char* reasons[] = {
    "NONE","TC_ERROR","OVERTEMP","RUNAWAY_DOWN","RUNAWAY_UP","WDG_TIMEOUT"
  };
  int idx = (int)reason;
  LOG_E(LOG_SAFETY, "!!! SHUTDOWN SICUREZZA: %s !!!\n",
        (idx >= 0 && idx <= 5) ? reasons[idx] : "?");
}

// ================================================================
//  NVS HELPERS
// ================================================================
static void nvs_load_to_state() {
  NVSData d;
  nvs->load(d);
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
  d.pct_cielo   = g_state.pct_cielo;
  g_state.nvs_dirty = false;
  MUTEX_GIVE();
  nvs->save(d);
}

// ================================================================
//  TASK_WATCHDOG
// ================================================================
#if TASK_WDG_ENABLE
static void Task_Watchdog(void* param) {
  LOG_I(LOG_WDG, "[Core %d] Task_Watchdog avviato (timeout=%dms)\n",
        xPortGetCoreID(), WDG_TIMEOUT_MS);

  // FIX: attendi 2s — Task_PID deve fare almeno un ciclo
  vTaskDelay(pdMS_TO_TICKS(2000));

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
      LOG_D(LOG_WDG, "[WDG] heartbeat ok hb=%lu\n", (unsigned long)hb);
      last_heartbeat = hb;
      last_beat_time = now;
    } else {
      uint32_t stall = now - last_beat_time;
      if (stall >= WDG_TIMEOUT_MS) {
        LOG_E(LOG_WDG, "[WDG] TIMEOUT! PID heartbeat fermo da %lums\n",
              (unsigned long)stall);
        emergency_shutdown(SafetyReason::WDG_TIMEOUT);
      } else {
        LOG_D(LOG_WDG, "[WDG] stall %lu/%dms\n",
              (unsigned long)stall, WDG_TIMEOUT_MS);
      }
    }
  }
}
#endif // TASK_WDG_ENABLE

// ================================================================
//  TASK_PID
// ================================================================
#if TASK_PID_ENABLE
static void Task_PID(void* param) {
  LOG_I(LOG_PID, "[Core %d] Task_PID avviato\n", xPortGetCoreID());

  unsigned long win_base  = 0;
  unsigned long win_cielo = 0;
  uint32_t      last_nvs_ms = millis();

  for (;;) {
    // FIX: heartbeat PRIMA di tutto
    g_pid_heartbeat++;

    uint32_t now = millis();

    if (g_emergency_shutdown) {
      RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
      RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
      bool fan = (g_state.temp_cielo > FAN_OFF_TEMP);
      if (fan != g_state.fan_on) {
        g_state.fan_on = fan;
        RELAY_WRITE(RELAY_FAN, RELAY_FAN_INV, fan);
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    // ── Lettura sensori ──
    float t_cielo_raw = tc_cielo->readCelsius();
    bool  err_cielo   = isnan(t_cielo_raw) || t_cielo_raw <= 0.0f;
    float t_base_raw  = 0.0f;
    bool  err_base    = false;

    if (g_state.sensor_mode == SensorMode::SINGLE) {
      t_base_raw = t_cielo_raw;
    } else {
      t_base_raw = tc_base->readCelsius();
      err_base   = isnan(t_base_raw) || t_base_raw <= 0.0f;
    }

    LOG_D(LOG_PID, "[PID] raw B=%.1f C=%.1f errB=%d errC=%d\n",
          t_base_raw, t_cielo_raw, err_base, err_cielo);

    // ── Gestione errore TC ──
    if (err_cielo) {
#if FEATURE_SAFETY
      if (g_state.sensor_mode == SensorMode::DUAL) {
        emergency_shutdown(SafetyReason::TC_ERROR);
        continue;
      }
#endif
      static uint32_t last_warn = 0;
      if (now - last_warn > 5000) {
        LOG_W(LOG_PID, "[PID] TC_CIELO ERR — open-loop\n");
        last_warn = now;
      }
      err_cielo = false;
    }

    if (err_base && g_state.sensor_mode == SensorMode::DUAL) {
#if FEATURE_SAFETY
      emergency_shutdown(SafetyReason::TC_ERROR);
      continue;
#endif
    }

    // ── Aggiorna stato ──
    if (MUTEX_TAKE()) {
      if (!err_cielo) g_state.temp_cielo = (double)t_cielo_raw;
      if (!err_base)  g_state.temp_base  = (double)t_base_raw;
      g_state.tc_cielo_err = err_cielo;
      g_state.tc_base_err  = err_base;
      MUTEX_GIVE();
    }

    // ── Overtemp ──
#if FEATURE_SAFETY
    if (g_state.temp_cielo > TEMP_MAX_SAFE || g_state.temp_base > TEMP_MAX_SAFE) {
      emergency_shutdown(SafetyReason::OVERTEMP);
      continue;
    }
#endif

    // ── PID + relay duty cycle ──
    bool base_on  = false;
    bool cielo_on = false;

    if (g_state.base_enabled) {
      pid_base->compute();
      uint32_t on_time = (uint32_t)(g_state.pid_out_base / 100.0 * 2000.0);
      on_time = constrain(on_time, 50u, 1950u);
      if (millis() - win_base >= 2000) win_base = millis();
      base_on = (millis() - win_base < on_time);
      LOG_D(LOG_PID, "[PID] base out=%.1f on=%lu base_on=%d\n",
            g_state.pid_out_base, (unsigned long)on_time, base_on);
    }

    if (g_state.cielo_enabled) {
      pid_cielo->compute();
      uint32_t on_time = (uint32_t)(g_state.pid_out_cielo / 100.0 * 2000.0);
      on_time = constrain(on_time, 50u, 1950u);
      if (millis() - win_cielo >= 2000) win_cielo = millis();
      cielo_on = (millis() - win_cielo < on_time);
    }

    RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  base_on);
    RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, cielo_on);

    if (MUTEX_TAKE()) {
      g_state.relay_base  = base_on;
      g_state.relay_cielo = cielo_on;
      bool fan = (g_state.temp_cielo > FAN_OFF_TEMP);
      if (fan != g_state.fan_on) {
        g_state.fan_on = fan;
        RELAY_WRITE(RELAY_FAN, RELAY_FAN_INV, fan);
      }
      MUTEX_GIVE();
    }

#if FEATURE_AUTOTUNE
    if (autotune_is_running()) {
      autotune_run(t_cielo_raw, now);
    }
#endif

    // ── NVS save periodico ──
    if (g_state.nvs_dirty && (now - last_nvs_ms > 5000)) {
      last_nvs_ms = now;
      nvs_save_from_state();
    }

    // ── Log PID periodico ──
    LOG_I(LOG_PID, "[C1 %5.1fs] %s B:%5.1f/%.0f %3.0f%%%c C:%5.1f/%.0f %3.0f%%%c\n",
      millis() / 1000.0f,
      g_state.sensor_mode == SensorMode::SINGLE ? "SGL" : "DUA",
      g_state.temp_base,  g_state.set_base,  g_state.pid_out_base,
      g_state.relay_base  ? '*' : '.',
      g_state.temp_cielo, g_state.set_cielo, g_state.pid_out_cielo,
      g_state.relay_cielo ? '*' : '.');

    vTaskDelay(pdMS_TO_TICKS(PID_SAMPLE_MS));
  }
}
#endif // TASK_PID_ENABLE

// ================================================================
//  TASK_LVGL
// ================================================================
#if TASK_LVGL_ENABLE
static void Task_LVGL(void* param) {
  LOG_I(LOG_LVGL, "[Core %d] Task_LVGL avviato\n", xPortGetCoreID());
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

        LOG_D(LOG_LVGL, "[LVGL] refresh screen=%d\n", (int)scr);

        switch (scr) {
          case Screen::MAIN:
            if (ui_ScreenMain && lv_scr_act() == ui_ScreenMain)
              ui_refresh(&g_state);
            break;
          case Screen::TEMP:
            if (ui_ScreenTemp && lv_scr_act() == ui_ScreenTemp)
              ui_refresh_temp(&g_state);
            break;
          case Screen::PID_BASE:
          case Screen::PID_CIELO:
            ui_refresh_pid(&g_state);
            break;
          case Screen::GRAPH:
            if (ui_ScreenGraph && lv_scr_act() == ui_ScreenGraph)
              ui_refresh_graph(&g_state);
            break;
#if TASK_WIFI_ENABLE
          case Screen::WIFI_SCAN:
            ui_wifi_update_list();
            ui_wifi_update_status();
            break;
          case Screen::OTA:
            ui_ota_update_progress();
            break;
          case Screen::WIFI_PWD:
            break;
#endif
          default:
            break;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
#endif // TASK_LVGL_ENABLE

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  LOG_I(LOG_SYSTEM, "\n=== FORNO PIZZA v17.0 — ESP32-S3 | ST7701A 480x272 ===\n");
  LOG_I(LOG_SYSTEM, "Task: PID=%d WDG=%d LVGL=%d WiFi=%d\n",
        TASK_PID_ENABLE, TASK_WDG_ENABLE, TASK_LVGL_ENABLE, TASK_WIFI_ENABLE);
  LOG_I(LOG_SYSTEM, "Feature: Splash=%d Safety=%d Autotune=%d OTA=%d\n",
        FEATURE_SPLASH, FEATURE_SAFETY, FEATURE_AUTOTUNE, FEATURE_OTA);

  // ---- 1. Relay OFF ----
  RELAY_INIT(RELAY_BASE,  RELAY_BASE_INV);
  RELAY_INIT(RELAY_CIELO, RELAY_CIELO_INV);
  RELAY_INIT(RELAY_LUCE,  RELAY_LUCE_INV);
  RELAY_INIT(RELAY_FAN,   RELAY_FAN_INV);

  // ---- 2. Display + LVGL ----
  display_init();

  // ---- 3. Splash ----
#if FEATURE_SPLASH
  splash_init();
  splash_set_progress(5, "Inizializzazione...");
#endif

  // ---- 4. Mutex ----
  g_mutex = xSemaphoreCreateMutex();
  if (!g_mutex) {
    LOG_E(LOG_SYSTEM, "ERRORE FATALE: mutex non creato!\n");
    while (true) vTaskDelay(1000);
  }
  LOG_I(LOG_SYSTEM, "[SETUP] Mutex OK\n");
#if FEATURE_SPLASH
  splash_set_progress(10, "Mutex OK");
#endif

  // ---- 5. GraphBuffer in PSRAM ----
  graph_alloc_psram();
#if FEATURE_SPLASH
  splash_set_progress(15, "Graph PSRAM OK");
#endif

  // ---- 6. Oggetti hardware (DOPO lcd.init) ----
  tc_base  = new MAX6675(TC_SCK, TC_CS_BASE,  TC_MISO);
  tc_cielo = new MAX6675(TC_SCK, TC_CS_CIELO, TC_MISO);
  nvs      = new NVSStorage();
  LOG_I(LOG_SYSTEM, "[SETUP] MAX6675 + NVS allocati\n");
#if FEATURE_SPLASH
  splash_set_progress(20, "Sensori OK");
#endif

  // ---- 7. NVS ----
  nvs_load_to_state();
  LOG_I(LOG_SYSTEM, "[SETUP] NVS: mode=%s set=%.0f/%.0f split=%d/%d\n",
    g_state.sensor_mode == SensorMode::SINGLE ? "SINGLE" : "DUAL",
    g_state.set_base, g_state.set_cielo,
    g_state.pct_base, g_state.pct_cielo);
#if FEATURE_SPLASH
  splash_set_progress(30, "NVS OK");
#endif

  // ---- 8. PID — allocato DOPO nvs_load (usa Kp/Ki/Kd corretti) ----
  pid_base  = new PIDController(&g_state.temp_base,  &g_state.pid_out_base,
                                 &g_state.set_base,
                                 g_state.kp_base,  g_state.ki_base,  g_state.kd_base);
  pid_cielo = new PIDController(&g_state.temp_cielo, &g_state.pid_out_cielo,
                                 &g_state.set_cielo,
                                 g_state.kp_cielo, g_state.ki_cielo, g_state.kd_cielo);
  pid_base->begin();
  pid_cielo->begin();
  LOG_I(LOG_SYSTEM, "[SETUP] PID OK (Kp=%.2f Ki=%.3f Kd=%.2f)\n",
        g_state.kp_base, g_state.ki_base, g_state.kd_base);
#if FEATURE_SPLASH
  splash_set_progress(40, "PID OK");
#endif

  // ---- 9. Prima lettura sensori ----
  delay(300);
  float t2 = tc_cielo->readCelsius();
  g_state.tc_cielo_err = isnan(t2) || t2 <= 0.0f;
  if (!g_state.tc_cielo_err) g_state.temp_cielo = (double)t2;
  if (g_state.sensor_mode == SensorMode::SINGLE) {
    g_state.tc_base_err = false;
    g_state.temp_base   = g_state.temp_cielo;
  } else {
    float t1 = tc_base->readCelsius();
    g_state.tc_base_err = isnan(t1) || t1 <= 0.0f;
    if (!g_state.tc_base_err) g_state.temp_base = (double)t1;
  }
  LOG_I(LOG_SYSTEM, "[SETUP] Temp: B=%.1f°C  C=%.1f°C\n",
        g_state.temp_base, g_state.temp_cielo);
#if FEATURE_SPLASH
  splash_set_progress(55, "Sensori OK");
#endif

  // ---- 10. UI ----
  ui_init();
  ui_refresh(&g_state);
  LOG_I(LOG_SYSTEM, "[SETUP] UI OK\n");
#if FEATURE_SPLASH
  splash_set_progress(70, "UI pronta");
#endif

  // ---- 11. WiFi ----
#if TASK_WIFI_ENABLE
  wifi_mqtt_init();
  LOG_I(LOG_SYSTEM, "[SETUP] WiFi init OK\n");
#else
  LOG_W(LOG_SYSTEM, "[SETUP] WiFi DISABILITATO\n");
#endif
#if FEATURE_SPLASH
  splash_set_progress(80, TASK_WIFI_ENABLE ? "WiFi OK" : "WiFi OFF");
#endif

  // ---- 12. OTA ----
#if FEATURE_OTA && TASK_WIFI_ENABLE
  ota_manager_init();
  web_ota_init();
  LOG_I(LOG_SYSTEM, "[SETUP] OTA OK\n");
#else
  LOG_W(LOG_SYSTEM, "[SETUP] OTA disabilitato\n");
#endif
#if FEATURE_SPLASH
  splash_set_progress(90, "Pronto");
#endif

  // ---- 13. Task FreeRTOS ----
  // FIX: g_pid_heartbeat=1 PRIMA di creare Task_Watchdog
  g_pid_heartbeat = 1;

#if TASK_WDG_ENABLE
  xTaskCreatePinnedToCore(Task_Watchdog, "Task_WDG",
    TASK_WDG_STACK, nullptr, TASK_WDG_PRIO, nullptr, TASK_WDG_CORE);
  LOG_I(LOG_SYSTEM, "[SETUP] Task_WDG avviato\n");
#else
  LOG_W(LOG_SYSTEM, "[SETUP] Task_WDG DISABILITATO\n");
#endif

#if TASK_PID_ENABLE
  xTaskCreatePinnedToCore(Task_PID, "Task_PID",
    TASK_PID_STACK, nullptr, TASK_PID_PRIO, nullptr, TASK_PID_CORE);
  LOG_I(LOG_SYSTEM, "[SETUP] Task_PID avviato\n");
#else
  LOG_W(LOG_SYSTEM, "[SETUP] Task_PID DISABILITATO — relay sempre OFF\n");
#endif

#if TASK_LVGL_ENABLE
  xTaskCreatePinnedToCore(Task_LVGL, "Task_LVGL",
    TASK_LVGL_STACK, nullptr, TASK_LVGL_PRIO, nullptr, TASK_LVGL_CORE);
  LOG_I(LOG_SYSTEM, "[SETUP] Task_LVGL avviato (stack=%d)\n", TASK_LVGL_STACK);
#else
  LOG_W(LOG_SYSTEM, "[SETUP] Task_LVGL DISABILITATO\n");
#endif

#if TASK_WIFI_ENABLE
  xTaskCreatePinnedToCore(Task_WiFi, "Task_WiFi",
    TASK_WIFI_STACK, nullptr, TASK_WIFI_PRIO, nullptr, TASK_WIFI_CORE);
  LOG_I(LOG_SYSTEM, "[SETUP] Task_WiFi avviato\n");
#endif

  LOG_I(LOG_SYSTEM, "[SETUP] Safety: TEMP_MAX=%.0f FAN_OFF=%.0f WDG=%dms\n",
        TEMP_MAX_SAFE, FAN_OFF_TEMP, WDG_TIMEOUT_MS);

  // ---- 14. Fine setup ----
#if FEATURE_SPLASH
  splash_finish(ui_ScreenMain);
#endif

  LOG_I(LOG_SYSTEM, "[SETUP] Avvio completato.\n");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
