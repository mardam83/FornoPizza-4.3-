/**
 * wifi_mqtt.cpp — Forno Pizza Controller v21-S3 + ANIMAZIONI
 * WiFi + MQTT + Home Assistant MQTT Discovery
 *
 * MODIFICA v21: aggiunta animazione blink WiFi durante connessione
 *   → anim_wifi_blink_start(ui_WifiStatusLbl)  prima di WiFi.begin()
 *   → anim_wifi_blink_stop(ui_WifiStatusLbl)   quando connesso/timeout
 *
 * Librerie richieste (Arduino Library Manager):
 *   - PubSubClient  by Nick O'Leary  (v2.8+)
 *   - ArduinoJson   by Benoit Blanchon (v6.x)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "wifi_mqtt.h"
#include "hardware.h"
#include "ui.h"
#include "autotune.h"
#include "ui_wifi.h"
#include "ui_animations.h"   // ← NUOVO v21: animazioni blink WiFi

// ================================================================
//  TOPIC helpers
// ================================================================
#define T_STATE       "forno/" MQTT_DEVICE_ID "/state"
#define T_AVAIL       "forno/" MQTT_DEVICE_ID "/availability"
#define T_SET_BASE    "forno/" MQTT_DEVICE_ID "/set/base"
#define T_SET_CIELO   "forno/" MQTT_DEVICE_ID "/set/cielo"
#define T_CMD_BASE    "forno/" MQTT_DEVICE_ID "/cmd/base"
#define T_CMD_CIELO   "forno/" MQTT_DEVICE_ID "/cmd/cielo"
#define T_CMD_LUCE    "forno/" MQTT_DEVICE_ID "/cmd/luce"
#define T_AT_CMD      "forno/" MQTT_DEVICE_ID "/autotune/cmd"
#define T_AT_SPLIT    "forno/" MQTT_DEVICE_ID "/autotune/split"
#define T_AT_STATUS   "forno/" MQTT_DEVICE_ID "/autotune/status"
#define HA_DISC       "homeassistant"

// ================================================================
//  STATO CONNESSIONE
// ================================================================
volatile bool g_wifi_connected = false;
volatile bool g_mqtt_connected = false;

// ================================================================
//  CREDENZIALI DINAMICHE
// ================================================================
static char  s_dyn_ssid[33] = "";
static char  s_dyn_pass[64] = "";
static volatile bool s_connect_request = false;

void wifi_request_connect(const char* ssid, const char* pass) {
  strncpy(s_dyn_ssid, ssid, 32); s_dyn_ssid[32] = '\0';
  strncpy(s_dyn_pass, pass, 63); s_dyn_pass[63] = '\0';

  // Salva in NVS per il prossimo boot
  Preferences prefs;
  prefs.begin("wifi_cred", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  s_connect_request = true;
}

// ================================================================
//  CLIENT MQTT
// ================================================================
static WiFiClient   wifi_client;
static PubSubClient mqtt(wifi_client);

// ================================================================
//  DISCOVERY helpers
// ================================================================
static void add_device(JsonObject& dev) {
  dev["identifiers"][0] = MQTT_DEVICE_ID;
  dev["name"]           = "Forno Pizza";
  dev["model"]          = "ESP32-S3 v21";
  dev["manufacturer"]   = "DIY";
}

// ================================================================
//  PUBLISH stato
// ================================================================
static void publish_state() {
  if (!mqtt.connected()) return;

  StaticJsonDocument<512> doc;
  bool locked = MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS);
  double temp_base  = g_state.temp_base;
  double temp_cielo = g_state.temp_cielo;
  double set_base   = g_state.set_base;
  double set_cielo  = g_state.set_cielo;
  double pid_base   = g_state.pid_out_base;
  double pid_cielo  = g_state.pid_out_cielo;
  bool base_en      = g_state.base_enabled;
  bool cielo_en     = g_state.cielo_enabled;
  bool luce_on      = g_state.luce_on;
  bool relay_b      = g_state.relay_base;
  bool relay_c      = g_state.relay_cielo;
  bool fan          = g_state.fan_on;
  bool shutdown     = g_state.safety_shutdown;
  SafetyReason reason = g_state.safety_reason;
  int pct_base      = g_state.pct_base;
  int pct_cielo     = g_state.pct_cielo;
  if (locked) MUTEX_GIVE();

  doc["temp_base"]   = serialized(String(temp_base,  1));
  doc["temp_cielo"]  = serialized(String(temp_cielo, 1));
  doc["set_base"]    = serialized(String(set_base,   0));
  doc["set_cielo"]   = serialized(String(set_cielo,  0));
  doc["pid_base"]    = serialized(String(pid_base,   1));
  doc["pid_cielo"]   = serialized(String(pid_cielo,  1));
  doc["base_en"]     = base_en  ? "ON" : "OFF";
  doc["cielo_en"]    = cielo_en ? "ON" : "OFF";
  doc["luce"]        = luce_on  ? "ON" : "OFF";
  doc["relay_base"]  = relay_b  ? "ON" : "OFF";
  doc["relay_cielo"] = relay_c  ? "ON" : "OFF";
  doc["fan"]         = fan      ? "ON" : "OFF";
  doc["shutdown"]    = shutdown;
  doc["pct_base"]    = pct_base;
  doc["pct_cielo"]   = pct_cielo;

  const char* reasons[] = {"OK","TC_ERROR","OVERTEMP","RUNAWAY_DOWN","RUNAWAY_UP","WDG_TIMEOUT"};
  int ri = (int)reason;
  doc["safety_reason"] = (ri >= 0 && ri <= 5) ? reasons[ri] : "UNKNOWN";

  const char* at_states[] = {"IDLE","RUNNING","DONE","ABORTED"};
  int ati = (int)g_state.autotune_status;
  doc["autotune_running"] = autotune_is_running();
  doc["autotune_status"]  = (ati >= 0 && ati <= 3) ? at_states[ati] : "UNKNOWN";
  doc["autotune_split"]   = g_state.autotune_split;
  doc["autotune_cycles"]  = g_state.autotune_cycles;

  char payload[512];
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(T_STATE, payload, false);
}

// ================================================================
//  MQTT callback
// ================================================================
static void mqtt_callback(char* topic_in, byte* pay, unsigned int len) {
  char msg[64] = {0};
  if (len >= sizeof(msg)) len = sizeof(msg) - 1;
  memcpy(msg, pay, len);

  if (strcmp(topic_in, T_SET_BASE) == 0) {
    float v = atof(msg);
    if (v >= 50 && v <= 500) {
      if (MUTEX_TAKE_MS(20)) { g_state.set_base = v; g_state.nvs_dirty = true; MUTEX_GIVE(); }
    }
    return;
  }
  if (strcmp(topic_in, T_SET_CIELO) == 0) {
    float v = atof(msg);
    if (v >= 50 && v <= 500) {
      if (MUTEX_TAKE_MS(20)) { g_state.set_cielo = v; g_state.nvs_dirty = true; MUTEX_GIVE(); }
    }
    return;
  }
  if (strcmp(topic_in, T_CMD_BASE) == 0) {
    bool on = (strcmp(msg, "ON") == 0);
    if (on && g_state.safety_shutdown) return;
    if (MUTEX_TAKE_MS(20)) { g_state.base_enabled = on; MUTEX_GIVE(); }
    return;
  }
  if (strcmp(topic_in, T_CMD_CIELO) == 0) {
    bool on = (strcmp(msg, "ON") == 0);
    if (on && g_state.safety_shutdown) return;
    if (MUTEX_TAKE_MS(20)) { g_state.cielo_enabled = on; MUTEX_GIVE(); }
    return;
  }
  if (strcmp(topic_in, T_AT_CMD) == 0) {
    if (strcmp(msg, "START") == 0 && !g_state.safety_shutdown && !autotune_is_running())
      autotune_start();
    else if (strcmp(msg, "STOP") == 0 && autotune_is_running())
      autotune_stop();
    return;
  }
  if (strcmp(topic_in, T_AT_SPLIT) == 0) {
    int val = atoi(msg);
    if (val >= 10 && val <= 90 && !autotune_is_running()) {
      if (MUTEX_TAKE_MS(20)) { g_state.autotune_split = val; MUTEX_GIVE(); }
    }
    return;
  }
  if (strcmp(topic_in, T_CMD_LUCE) == 0) {
    bool on = (strcmp(msg, "ON") == 0);
    if (MUTEX_TAKE_MS(20)) { g_state.luce_on = on; MUTEX_GIVE(); }
    return;
  }
}

// ================================================================
//  MQTT connect + discovery
// ================================================================
static bool mqtt_connect() {
  char client_id[32];
  snprintf(client_id, sizeof(client_id), "%s_%04X", MQTT_DEVICE_ID,
           (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

  bool ok;
  if (strlen(MQTT_USER) > 0)
    ok = mqtt.connect(client_id, MQTT_USER, MQTT_PASS, T_AVAIL, 1, true, "offline");
  else
    ok = mqtt.connect(client_id, nullptr, nullptr, T_AVAIL, 1, true, "offline");

  if (!ok) return false;

  mqtt.publish(T_AVAIL, "online", true);
  mqtt.subscribe(T_SET_BASE);
  mqtt.subscribe(T_SET_CIELO);
  mqtt.subscribe(T_CMD_BASE);
  mqtt.subscribe(T_CMD_CIELO);
  mqtt.subscribe(T_CMD_LUCE);
  mqtt.subscribe(T_AT_CMD);
  mqtt.subscribe(T_AT_SPLIT);

  g_mqtt_connected = true;
  Serial.println("[MQTT] Connesso e iscritto ai topic");
  return true;
}

// ================================================================
//  TASK_WIFI — Core 0, priorità 1
//  MODIFICA v21: blink WiFi animato durante tentativo connessione
// ================================================================
void Task_WiFi(void* param) {
  Serial.printf("[Core %d] Task_WiFi avviato\n", xPortGetCoreID());

  uint32_t last_publish_ms  = 0;
  uint32_t last_wifi_try_ms = 0;
  uint32_t last_mqtt_try_ms = 0;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqtt_callback);
  mqtt.setKeepAlive(MQTT_KEEPALIVE);
  mqtt.setBufferSize(640);

  // Flag locale per tracciare lo stato del blink
  static bool s_blink_active = false;

  for (;;) {
    uint32_t now = millis();

    // ── Scan WiFi richiesto dall'UI ──
    if (g_wifi_scan_request) {
      g_wifi_scan_request = false;
      g_wifi_net_count    = -1;
      int n = WiFi.scanNetworks(false, true);
      int count = (n > WIFI_SCAN_MAX) ? WIFI_SCAN_MAX : n;
      for (int i = 0; i < count; i++) {
        strncpy(g_wifi_nets[i].ssid, WiFi.SSID(i).c_str(), 32);
        g_wifi_nets[i].ssid[32] = '\0';
        g_wifi_nets[i].rssi   = WiFi.RSSI(i);
        g_wifi_nets[i].secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      }
      g_wifi_net_count = count;
      WiFi.scanDelete();
    }

    // ── Connessione con nuove credenziali (richiesta da UI) ──
    if (s_connect_request) {
      s_connect_request = false;
      WiFi.disconnect(false);
      vTaskDelay(pdMS_TO_TICKS(400));
      last_wifi_try_ms = 0;
    }

    // ── WiFi non connesso ──
    if (WiFi.status() != WL_CONNECTED) {
      if (g_wifi_connected) {
        // Era connesso, ora non lo è più → ferma blink se era attivo
        g_wifi_connected = false;
        g_mqtt_connected = false;
        if (s_blink_active) {
          anim_wifi_blink_stop(ui_WifiStatusLbl);    // ← ANIMAZIONE 6: stop
          s_blink_active = false;
        }
        ui_wifi_update_status();
      }

      if (now - last_wifi_try_ms >= WIFI_RETRY_MS) {
        last_wifi_try_ms = now;
        const char* use_ssid = strlen(s_dyn_ssid) ? s_dyn_ssid : WIFI_SSID;
        const char* use_pass = strlen(s_dyn_ssid) ? s_dyn_pass : WIFI_PASS;
        Serial.printf("[WiFi] Connessione a %s...\n", use_ssid);

        // ← ANIMAZIONE 6: avvia blink durante tentativo
        if (ui_WifiStatusLbl && !s_blink_active) {
          lv_label_set_text(ui_WifiStatusLbl, LV_SYMBOL_WIFI " Connessione...");
          lv_obj_set_style_text_color(ui_WifiStatusLbl, lv_color_make(0xFF,0xE0,0x00), 0);
          anim_wifi_blink_start(ui_WifiStatusLbl);
          s_blink_active = true;
        }

        WiFi.begin(use_ssid, use_pass);
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000)
          vTaskDelay(pdMS_TO_TICKS(200));

        if (WiFi.status() == WL_CONNECTED) {
          g_wifi_connected = true;
          // ← ANIMAZIONE 6: stop blink → connesso
          if (s_blink_active) {
            anim_wifi_blink_stop(ui_WifiStatusLbl);
            s_blink_active = false;
          }
          ui_wifi_update_status();
          Serial.printf("[WiFi] Connesso! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
          // ← Timeout: ferma blink anche in caso di fallimento
          if (s_blink_active) {
            anim_wifi_blink_stop(ui_WifiStatusLbl);
            s_blink_active = false;
          }
          if (ui_WifiStatusLbl) {
            lv_label_set_text(ui_WifiStatusLbl, LV_SYMBOL_CLOSE " Timeout connessione");
            lv_obj_set_style_text_color(ui_WifiStatusLbl, lv_color_make(0xFF,0x40,0x40), 0);
          }
          Serial.println("[WiFi] Timeout");
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // ── WiFi connesso ──
    if (!g_wifi_connected) {
      g_wifi_connected = true;
      if (s_blink_active) {
        anim_wifi_blink_stop(ui_WifiStatusLbl);      // ← ANIMAZIONE 6: stop finale
        s_blink_active = false;
      }
      ui_wifi_update_status();
    }

    // ── MQTT ──
    if (!mqtt.connected()) {
      g_mqtt_connected = false;
      if (now - last_mqtt_try_ms >= MQTT_RETRY_MS) {
        last_mqtt_try_ms = now;
        mqtt_connect();
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    mqtt.loop();

    if (now - last_publish_ms >= MQTT_PUBLISH_MS) {
      last_publish_ms = now;
      publish_state();
    }

    static bool last_shutdown = false;
    if (g_state.safety_shutdown && !last_shutdown) {
      publish_state();
      last_shutdown = true;
    }
    if (!g_state.safety_shutdown) last_shutdown = false;

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ================================================================
//  wifi_mqtt_init
// ================================================================
void wifi_mqtt_init() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());

  Preferences prefs;
  prefs.begin("wifi_cred", true);
  String saved_ssid = prefs.getString("ssid", "");
  String saved_pass = prefs.getString("pass", "");
  prefs.end();
  if (saved_ssid.length() > 0) {
    strncpy(s_dyn_ssid, saved_ssid.c_str(), 32);
    strncpy(s_dyn_pass, saved_pass.c_str(), 63);
    Serial.printf("[WiFi] Credenziali NVS: %s\n", s_dyn_ssid);
  }
}
