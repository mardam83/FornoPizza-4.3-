/**
 * wifi_mqtt.cpp — Forno Pizza Controller v23-S3
 * WiFi + MQTT + Home Assistant MQTT Discovery
 *
 * MODIFICA v23: FIX thread-safety WiFi scan + UI
 *   PROBLEMA: Task_WiFi chiamava direttamente funzioni LVGL
 *             (lv_label_set_text, anim_wifi_blink_start/stop,
 *              ui_wifi_update_status, ui_wifi_update_list)
 *             mentre Task_LVGL renderizzava → "_lv_inv_area: detected
 *             modifying dirty areas in render" crash.
 *
 *   SOLUZIONE:
 *     1. Scan WiFi asincrono (WiFi.scanNetworks(true)) — non blocca il task
 *     2. Task_WiFi scrive SOLO flag volatili: g_wifi_scan_done,
 *        g_wifi_status_changed
 *     3. Task_LVGL legge i flag e chiama le funzioni UI in sicurezza
 *     4. ZERO chiamate LVGL dirette da Task_WiFi
 *
 * MODIFICA v21: aggiunta animazione blink WiFi durante connessione
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
#include "ui_animations.h"

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

#define MQTT_NVS_NAMESPACE  "mqtt"
#define MQTT_DEFAULT_PORT   "1883"

// ================================================================
//  CONFIG MQTT DA NVS (stessi valori salvati dalla schermata MQTT)
// ================================================================
static char s_mqtt_host[65] = "192.168.1.100";
static char s_mqtt_port[8]  = "1883";
static char s_mqtt_user[33] = "";
static char s_mqtt_pass[33] = "";

static void mqtt_config_load() {
  Preferences prefs;
  if (!prefs.begin(MQTT_NVS_NAMESPACE, true)) {
    strncpy(s_mqtt_host, "192.168.1.100", sizeof(s_mqtt_host) - 1); s_mqtt_host[sizeof(s_mqtt_host)-1] = '\0';
    strncpy(s_mqtt_port, MQTT_DEFAULT_PORT, sizeof(s_mqtt_port) - 1); s_mqtt_port[sizeof(s_mqtt_port)-1] = '\0';
    s_mqtt_user[0] = s_mqtt_pass[0] = '\0';
    prefs.end();
    return;
  }
  strncpy(s_mqtt_host, prefs.getString("host", "192.168.1.100").c_str(), sizeof(s_mqtt_host) - 1); s_mqtt_host[sizeof(s_mqtt_host)-1] = '\0';
  strncpy(s_mqtt_port, prefs.getString("port", MQTT_DEFAULT_PORT).c_str(), sizeof(s_mqtt_port) - 1); s_mqtt_port[sizeof(s_mqtt_port)-1] = '\0';
  strncpy(s_mqtt_user, prefs.getString("user", "").c_str(), sizeof(s_mqtt_user) - 1); s_mqtt_user[sizeof(s_mqtt_user)-1] = '\0';
  strncpy(s_mqtt_pass, prefs.getString("pass", "").c_str(), sizeof(s_mqtt_pass) - 1); s_mqtt_pass[sizeof(s_mqtt_pass)-1] = '\0';
  prefs.end();
}

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
static void add_device(JsonObject dev) {
  dev["identifiers"][0] = MQTT_DEVICE_ID;
  dev["name"]           = MQTT_DEVICE_NAME;
  dev["manufacturer"]   = "DIY";
  dev["model"]          = "ESP32-S3 Forno Pizza";
}

static void pub_discovery() {
  // Helper macro per pubblicare config HA discovery
  StaticJsonDocument<512> doc;
  char topic[128], payload[512];

  // ── Sensore temperatura base ──
  doc.clear();
  doc["name"]                = "Temp Base";
  doc["unique_id"]           = MQTT_DEVICE_ID "_temp_base";
  doc["state_topic"]         = T_STATE;
  doc["value_template"]      = "{{ value_json.temp_base }}";
  doc["unit_of_measurement"] = "°C";
  doc["device_class"]        = "temperature";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/sensor/%s_temp_base/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Sensore temperatura cielo ──
  doc.clear();
  doc["name"]                = "Temp Cielo";
  doc["unique_id"]           = MQTT_DEVICE_ID "_temp_cielo";
  doc["state_topic"]         = T_STATE;
  doc["value_template"]      = "{{ value_json.temp_cielo }}";
  doc["unit_of_measurement"] = "°C";
  doc["device_class"]        = "temperature";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/sensor/%s_temp_cielo/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Switch relay base ──
  doc.clear();
  doc["name"]           = "Resistenza Base";
  doc["unique_id"]      = MQTT_DEVICE_ID "_base";
  doc["state_topic"]    = T_STATE;
  doc["value_template"] = "{{ value_json.base }}";
  doc["command_topic"]  = T_CMD_BASE;
  doc["payload_on"]     = "ON";
  doc["payload_off"]    = "OFF";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/switch/%s_base/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Switch relay cielo ──
  doc.clear();
  doc["name"]           = "Resistenza Cielo";
  doc["unique_id"]      = MQTT_DEVICE_ID "_cielo";
  doc["state_topic"]    = T_STATE;
  doc["value_template"] = "{{ value_json.cielo }}";
  doc["command_topic"]  = T_CMD_CIELO;
  doc["payload_on"]     = "ON";
  doc["payload_off"]    = "OFF";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/switch/%s_cielo/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Switch luce ──
  doc.clear();
  doc["name"]           = "Luce Forno";
  doc["unique_id"]      = MQTT_DEVICE_ID "_luce";
  doc["state_topic"]    = T_STATE;
  doc["value_template"] = "{{ value_json.luce }}";
  doc["command_topic"]  = T_CMD_LUCE;
  doc["payload_on"]     = "ON";
  doc["payload_off"]    = "OFF";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/switch/%s_luce/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Number setpoint base ──
  doc.clear();
  doc["name"]           = "Setpoint Base";
  doc["unique_id"]      = MQTT_DEVICE_ID "_set_base";
  doc["state_topic"]    = T_STATE;
  doc["value_template"] = "{{ value_json.set_base }}";
  doc["command_topic"]  = T_SET_BASE;
  doc["min"]            = 50;
  doc["max"]            = 500;
  doc["step"]           = 5;
  doc["unit_of_measurement"] = "°C";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/number/%s_set_base/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Number setpoint cielo ──
  doc.clear();
  doc["name"]           = "Setpoint Cielo";
  doc["unique_id"]      = MQTT_DEVICE_ID "_set_cielo";
  doc["state_topic"]    = T_STATE;
  doc["value_template"] = "{{ value_json.set_cielo }}";
  doc["command_topic"]  = T_SET_CIELO;
  doc["min"]            = 50;
  doc["max"]            = 500;
  doc["step"]           = 5;
  doc["unit_of_measurement"] = "°C";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/number/%s_set_cielo/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Binary sensor ventola ──
  doc.clear();
  doc["name"]           = "Ventola";
  doc["unique_id"]      = MQTT_DEVICE_ID "_fan";
  doc["state_topic"]    = T_STATE;
  doc["value_template"] = "{{ value_json.fan }}";
  doc["payload_on"]     = "ON";
  doc["payload_off"]    = "OFF";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/binary_sensor/%s_fan/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ── Binary sensor safety ──
  doc.clear();
  doc["name"]           = "Safety Shutdown";
  doc["unique_id"]      = MQTT_DEVICE_ID "_safety";
  doc["state_topic"]    = T_STATE;
  doc["value_template"] = "{{ value_json.shutdown }}";
  doc["payload_on"]     = true;
  doc["payload_off"]    = false;
  doc["device_class"]   = "problem";
  add_device(doc["device"].to<JsonObject>());
  snprintf(topic, sizeof(topic), "%s/binary_sensor/%s_safety/config", HA_DISC, MQTT_DEVICE_ID);
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  Serial.println("[MQTT] Discovery pubblicato");
}

// ================================================================
//  PUBLISH STATE
// ================================================================
static void publish_state() {
  if (!mqtt.connected()) return;

  bool base, cielo, luce, fan, shutdown;
  float temp_b, temp_c, set_b, set_c, pid_b, pid_c;
  int pct_base, pct_cielo;
  SafetyReason reason;

  if (!MUTEX_TAKE_MS(20)) return;
  base      = g_state.base_enabled  && g_state.relay_base;
  cielo     = g_state.cielo_enabled && g_state.relay_cielo;
  luce      = g_state.luce_on;
  fan       = g_state.fan_on;
  shutdown  = g_state.safety_shutdown;
  temp_b    = g_state.temp_base;
  temp_c    = g_state.temp_cielo;
  set_b     = g_state.set_base;
  set_c     = g_state.set_cielo;
  pid_b     = g_state.pid_out_base;
  pid_c     = g_state.pid_out_cielo;
  pct_base  = (int)g_state.pid_out_base;
  pct_cielo = (int)g_state.pid_out_cielo;
  reason    = g_state.safety_reason;
  MUTEX_GIVE();

  StaticJsonDocument<512> doc;
  doc["temp_base"]  = serialized(String(temp_b, 1));
  doc["temp_cielo"] = serialized(String(temp_c, 1));
  doc["set_base"]   = serialized(String(set_b,  1));
  doc["set_cielo"]  = serialized(String(set_c,  1));
  doc["pid_base"]   = serialized(String(pid_b,  1));
  doc["pid_cielo"]  = serialized(String(pid_c,  1));
  doc["base"]       = base     ? "ON" : "OFF";
  doc["cielo"]      = cielo    ? "ON" : "OFF";
  doc["luce"]       = luce     ? "ON" : "OFF";
  doc["fan"]        = fan      ? "ON" : "OFF";
  doc["shutdown"]   = shutdown;
  doc["pct_base"]   = pct_base;
  doc["pct_cielo"]  = pct_cielo;

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
//  MQTT connect + discovery (usa host/port/user/pass da NVS)
// ================================================================
static bool mqtt_connect() {
  mqtt_config_load();
  int port = atoi(s_mqtt_port);
  if (port <= 0 || port > 65535) port = 1883;
  mqtt.setServer(s_mqtt_host, (uint16_t)port);

  char client_id[32];
  snprintf(client_id, sizeof(client_id), "%s_%04X", MQTT_DEVICE_ID,
           (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

  bool ok;
  if (strlen(s_mqtt_user) > 0)
    ok = mqtt.connect(client_id, s_mqtt_user, s_mqtt_pass, T_AVAIL, 1, true, "offline");
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
  pub_discovery();
  return true;
}

// ================================================================
//  TASK_WIFI — Core 0, priorità 1
//
//  FIX v23: ZERO chiamate LVGL dirette.
//  Task_WiFi scrive solo flag volatili:
//    g_wifi_scan_done      → Task_LVGL chiama ui_wifi_update_list()
//    g_wifi_status_changed → Task_LVGL chiama ui_wifi_update_status()
//  Le animazioni blink vengono gestite tramite i flag — Task_LVGL
//  le avvia/ferma nel contesto corretto.
// ================================================================
void Task_WiFi(void* param) {
  Serial.printf("[Core %d] Task_WiFi avviato\n", xPortGetCoreID());

  uint32_t last_publish_ms  = 0;
  uint32_t last_wifi_try_ms = 0;
  uint32_t last_mqtt_try_ms = 0;

  mqtt_config_load();
  int port = atoi(s_mqtt_port);
  if (port <= 0 || port > 65535) port = 1883;
  mqtt.setServer(s_mqtt_host, (uint16_t)port);
  mqtt.setCallback(mqtt_callback);
  mqtt.setKeepAlive(MQTT_KEEPALIVE);
  mqtt.setBufferSize(640);

  // Stato locale per rilevare transizioni (non tocca LVGL)
  bool was_connected = false;

  for (;;) {
    uint32_t now = millis();

    // ── Scan WiFi asincrono — richiesto dall'UI ──────────────────
    // FIX v23: usa scanNetworks(async=true) — non blocca il task
    if (g_wifi_scan_request) {
      g_wifi_scan_request = false;
      g_wifi_net_count    = -1;        // segnala "in corso" all'UI
      g_wifi_scan_done    = false;
      WiFi.scanDelete();               // pulisce risultati precedenti
      WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/true);
      Serial.println("[WiFi] Scan avviato (asincrono)");
    }

    // ── Controlla risultato scan asincrono ───────────────────────
    if (g_wifi_net_count == -1) {
      int n = WiFi.scanComplete();
      if (n >= 0) {
        // Scan completato — popola array (no LVGL, solo dati)
        int count = (n > WIFI_SCAN_MAX) ? WIFI_SCAN_MAX : n;
        for (int i = 0; i < count; i++) {
          strncpy(g_wifi_nets[i].ssid, WiFi.SSID(i).c_str(), 32);
          g_wifi_nets[i].ssid[32] = '\0';
          g_wifi_nets[i].rssi     = WiFi.RSSI(i);
          g_wifi_nets[i].secure   = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
        g_wifi_net_count = count;
        WiFi.scanDelete();
        g_wifi_scan_done = true;   // ← Task_LVGL chiamerà ui_wifi_update_list()
        Serial.printf("[WiFi] Scan completato: %d reti\n", count);
      } else if (n == WIFI_SCAN_FAILED) {
        g_wifi_net_count = 0;
        g_wifi_scan_done = true;   // mostra "Nessuna rete"
        Serial.println("[WiFi] Scan FALLITO");
      }
      // n == WIFI_SCAN_RUNNING(-1) → ancora in corso, aspetta
    }

    // ── Connessione con nuove credenziali (richiesta da UI) ──────
    if (s_connect_request) {
      s_connect_request = false;
      WiFi.disconnect(false);
      vTaskDelay(pdMS_TO_TICKS(400));
      last_wifi_try_ms = 0;
    }

    // ── WiFi non connesso ────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
      if (was_connected) {
        was_connected    = false;
        g_wifi_connected = false;
        g_mqtt_connected = false;
        g_wifi_status_changed = true;  // ← Task_LVGL aggiorna UI
        Serial.println("[WiFi] Disconnesso");
      }

      if (now - last_wifi_try_ms >= WIFI_RETRY_MS) {
        last_wifi_try_ms = now;

        // Se non ci sono credenziali in NVS non tentare — aspetta
        // che l'utente scelga la rete dalla schermata WiFi Scan.
        if (strlen(s_dyn_ssid) == 0) {
          Serial.println("[WiFi] Nessuna credenziale — usa WiFi Scan per scegliere la rete");
          vTaskDelay(pdMS_TO_TICKS(500));
          continue;
        }

        Serial.printf("[WiFi] Connessione a %s...\n", s_dyn_ssid);
        g_wifi_status_changed = true;

        WiFi.begin(s_dyn_ssid, s_dyn_pass);
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000)
          vTaskDelay(pdMS_TO_TICKS(200));

        if (WiFi.status() == WL_CONNECTED) {
          was_connected    = true;
          g_wifi_connected = true;
          g_wifi_status_changed = true;  // ← Task_LVGL aggiorna UI
          Serial.printf("[WiFi] Connesso! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
          Serial.println("[WiFi] Timeout");
          g_wifi_status_changed = true;  // ← aggiorna label "Non connesso"
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // ── WiFi connesso ─────────────────────────────────────────────
    if (!was_connected) {
      was_connected    = true;
      g_wifi_connected = true;
      g_wifi_status_changed = true;  // ← Task_LVGL aggiorna UI
    }

    // ── MQTT ──────────────────────────────────────────────────────
    if (!mqtt.connected()) {
      if (g_mqtt_connected) {
        g_mqtt_connected      = false;
        g_wifi_status_changed = true;
      }
      if (now - last_mqtt_try_ms >= MQTT_RETRY_MS) {
        last_mqtt_try_ms = now;
        mqtt_connect();
        if (g_mqtt_connected)
          g_wifi_status_changed = true;
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
