/**
 * wifi_mqtt.cpp — Forno Pizza Controller v14
 * WiFi + MQTT + Home Assistant MQTT Discovery
 *
 * Librerie richieste (installare da Arduino Library Manager):
 *   - PubSubClient  by Nick O'Leary  (v2.8+)
 *   - ArduinoJson   by Benoit Blanchon (v6.x)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>     // v19: salvataggio credenziali WiFi da UI
#include "wifi_mqtt.h"
#include "hardware.h"
#include "ui.h"
#include "autotune.h"
#include "ui_wifi.h"         // v19: WiFi scan + OTA flags

// ================================================================
//  TOPIC helpers — costruiti da MQTT_DEVICE_ID
// ================================================================
// Pubblicazione stato
#define T_STATE       "forno/" MQTT_DEVICE_ID "/state"
#define T_AVAIL       "forno/" MQTT_DEVICE_ID "/availability"

// Comandi in entrata
#define T_SET_BASE    "forno/" MQTT_DEVICE_ID "/set/base"
#define T_SET_CIELO   "forno/" MQTT_DEVICE_ID "/set/cielo"
#define T_CMD_BASE    "forno/" MQTT_DEVICE_ID "/cmd/base"
#define T_CMD_CIELO   "forno/" MQTT_DEVICE_ID "/cmd/cielo"
#define T_CMD_LUCE    "forno/" MQTT_DEVICE_ID "/cmd/luce"

// Autotune
#define T_AT_CMD      "forno/" MQTT_DEVICE_ID "/autotune/cmd"      // START / STOP
#define T_AT_SPLIT    "forno/" MQTT_DEVICE_ID "/autotune/split"    // 10-90 (%)
#define T_AT_STATUS   "forno/" MQTT_DEVICE_ID "/autotune/status"   // pub: stato

// Discovery base path
#define HA_DISC       "homeassistant"

// ================================================================
//  STATO CONNESSIONE (visibile in ui per icona WiFi)
// ================================================================
volatile bool g_wifi_connected = false;
volatile bool g_mqtt_connected = false;

// ================================================================
//  CREDENZIALI DINAMICHE — impostate dall'UI WiFi Scan (v19)
// ================================================================
static char  s_dyn_ssid[33] = "";
static char  s_dyn_pass[64] = "";
static volatile bool s_connect_request = false;

// Chiamata dall'UI per richiedere connessione a rete selezionata
void wifi_request_connect(const char* ssid, const char* pass) {
  strncpy(s_dyn_ssid, ssid, 32); s_dyn_ssid[32] = '\0';
  strncpy(s_dyn_pass, pass, 63); s_dyn_pass[63] = '\0';
  s_connect_request = true;
  // Salva in NVS — sopravvive al riavvio
  Preferences prefs;
  prefs.begin("wifi_cred", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  Serial.printf("[WiFi] Richiesta connessione a: %s\n", ssid);
}

// ================================================================
//  CLIENT
// ================================================================
static WiFiClient   wifi_client;
static PubSubClient mqtt(wifi_client);

// ================================================================
//  DEVICE JSON per Discovery
// ================================================================
static void add_device(JsonObject& dev) {
  JsonArray ids = dev.createNestedArray("identifiers");
  ids.add(MQTT_DEVICE_ID);
  dev["name"]         = MQTT_DEVICE_NAME;
  dev["model"]        = "ESP32-2432S028R";
  dev["manufacturer"] = "Custom";
  dev["sw_version"]   = "14.0";
}

// ================================================================
//  PUBLISH DISCOVERY
//  Registra tutte le entità in HA — chiamato una volta alla connessione
// ================================================================
static void publish_discovery() {
  StaticJsonDocument<512> doc;
  char topic[128];
  char payload[512];

  // Macro helper per ridurre ripetizioni
  #define DISC_SENSOR(uid, name, unit, icon, value_template) { \
    doc.clear(); \
    snprintf(topic, sizeof(topic), HA_DISC "/sensor/" MQTT_DEVICE_ID "_%s/config", uid); \
    doc["unique_id"]       = MQTT_DEVICE_ID "_" uid; \
    doc["name"]            = name; \
    doc["state_topic"]     = T_STATE; \
    doc["availability_topic"] = T_AVAIL; \
    doc["unit_of_measurement"] = unit; \
    doc["icon"]            = icon; \
    doc["value_template"]  = value_template; \
    doc["device_class"]    = "temperature"; \
    JsonObject dev = doc.createNestedObject("device"); add_device(dev); \
    serializeJson(doc, payload, sizeof(payload)); \
    mqtt.publish(topic, payload, true); \
  }

  #define DISC_SENSOR_PLAIN(uid, name, unit, icon, value_template) { \
    doc.clear(); \
    snprintf(topic, sizeof(topic), HA_DISC "/sensor/" MQTT_DEVICE_ID "_%s/config", uid); \
    doc["unique_id"]       = MQTT_DEVICE_ID "_" uid; \
    doc["name"]            = name; \
    doc["state_topic"]     = T_STATE; \
    doc["availability_topic"] = T_AVAIL; \
    doc["unit_of_measurement"] = unit; \
    doc["icon"]            = icon; \
    doc["value_template"]  = value_template; \
    JsonObject dev = doc.createNestedObject("device"); add_device(dev); \
    serializeJson(doc, payload, sizeof(payload)); \
    mqtt.publish(topic, payload, true); \
  }

  // ---- Sensori temperatura ----
  DISC_SENSOR("temp_base",  "Forno - Temp Base",  "°C", "mdi:thermometer",
              "{{ value_json.temp_base | round(1) }}");
  DISC_SENSOR("temp_cielo", "Forno - Temp Cielo", "°C", "mdi:thermometer",
              "{{ value_json.temp_cielo | round(1) }}");

  // ---- Setpoint (sensor, read-only — i number gestiscono la scrittura) ----
  DISC_SENSOR_PLAIN("set_base",  "Forno - Setpoint Base",  "°C", "mdi:target",
                    "{{ value_json.set_base | int }}");
  DISC_SENSOR_PLAIN("set_cielo", "Forno - Setpoint Cielo", "°C", "mdi:target",
                    "{{ value_json.set_cielo | int }}");

  // ---- PID output ----
  DISC_SENSOR_PLAIN("pid_base",  "Forno - PID Base",  "%", "mdi:gauge",
                    "{{ value_json.pid_base | round(0) }}");
  DISC_SENSOR_PLAIN("pid_cielo", "Forno - PID Cielo", "%", "mdi:gauge",
                    "{{ value_json.pid_cielo | round(0) }}");

  // ---- Allarme sicurezza ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/sensor/" MQTT_DEVICE_ID "_safety/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_safety";
  doc["name"]            = "Forno - Allarme";
  doc["state_topic"]     = T_STATE;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:alert";
  doc["value_template"]  = "{{ value_json.safety_reason }}";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Binary sensor ventola ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/binary_sensor/" MQTT_DEVICE_ID "_fan/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_fan";
  doc["name"]            = "Forno - Ventola";
  doc["state_topic"]     = T_STATE;
  doc["availability_topic"] = T_AVAIL;
  doc["device_class"]    = "running";
  doc["icon"]            = "mdi:fan";
  doc["value_template"]  = "{{ 'ON' if value_json.fan else 'OFF' }}";
  doc["payload_on"]      = "ON";
  doc["payload_off"]     = "OFF";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Binary sensor safety shutdown ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/binary_sensor/" MQTT_DEVICE_ID "_shutdown/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_shutdown";
  doc["name"]            = "Forno - Safety Shutdown";
  doc["state_topic"]     = T_STATE;
  doc["availability_topic"] = T_AVAIL;
  doc["device_class"]    = "problem";
  doc["icon"]            = "mdi:shield-alert";
  doc["value_template"]  = "{{ 'ON' if value_json.shutdown else 'OFF' }}";
  doc["payload_on"]      = "ON";
  doc["payload_off"]     = "OFF";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Switch relay BASE ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/switch/" MQTT_DEVICE_ID "_base/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_base";
  doc["name"]            = "Forno - Resistenza Base";
  doc["state_topic"]     = T_STATE;
  doc["command_topic"]   = T_CMD_BASE;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:heating-coil";
  doc["value_template"]  = "{{ 'ON' if value_json.base_en else 'OFF' }}";
  doc["state_on"]        = "ON";
  doc["state_off"]       = "OFF";
  doc["payload_on"]      = "ON";
  doc["payload_off"]     = "OFF";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Switch relay CIELO ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/switch/" MQTT_DEVICE_ID "_cielo/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_cielo";
  doc["name"]            = "Forno - Resistenza Cielo";
  doc["state_topic"]     = T_STATE;
  doc["command_topic"]   = T_CMD_CIELO;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:heating-coil";
  doc["value_template"]  = "{{ 'ON' if value_json.cielo_en else 'OFF' }}";
  doc["state_on"]        = "ON";
  doc["state_off"]       = "OFF";
  doc["payload_on"]      = "ON";
  doc["payload_off"]     = "OFF";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Switch LUCE ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/switch/" MQTT_DEVICE_ID "_luce/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_luce";
  doc["name"]            = "Forno - Luce";
  doc["state_topic"]     = T_STATE;
  doc["command_topic"]   = T_CMD_LUCE;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:lightbulb";
  doc["value_template"]  = "{{ 'ON' if value_json.luce else 'OFF' }}";
  doc["state_on"]        = "ON";
  doc["state_off"]       = "OFF";
  doc["payload_on"]      = "ON";
  doc["payload_off"]     = "OFF";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Number setpoint BASE ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/number/" MQTT_DEVICE_ID "_set_base/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_set_base";
  doc["name"]            = "Forno - Setpoint Base";
  doc["state_topic"]     = T_STATE;
  doc["command_topic"]   = T_SET_BASE;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:thermometer-chevron-up";
  doc["unit_of_measurement"] = "°C";
  doc["value_template"]  = "{{ value_json.set_base | int }}";
  doc["min"]             = 50;
  doc["max"]             = 450;
  doc["step"]            = 5;
  doc["mode"]            = "slider";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Number setpoint CIELO ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/number/" MQTT_DEVICE_ID "_set_cielo/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_set_cielo";
  doc["name"]            = "Forno - Setpoint Cielo";
  doc["state_topic"]     = T_STATE;
  doc["command_topic"]   = T_SET_CIELO;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:thermometer-chevron-up";
  doc["unit_of_measurement"] = "°C";
  doc["value_template"]  = "{{ value_json.set_cielo | int }}";
  doc["min"]             = 50;
  doc["max"]             = 450;
  doc["step"]            = 5;
  doc["mode"]            = "slider";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Autotune switch ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/switch/" MQTT_DEVICE_ID "_autotune/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_autotune";
  doc["name"]            = "Forno - Autotune PID";
  doc["state_topic"]     = T_STATE;
  doc["command_topic"]   = T_AT_CMD;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:auto-fix";
  doc["value_template"]  = "{{ 'ON' if value_json.autotune_running else 'OFF' }}";
  doc["payload_on"]      = "START";
  doc["payload_off"]     = "STOP";
  doc["state_on"]        = "ON";
  doc["state_off"]       = "OFF";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Autotune split (number) ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/number/" MQTT_DEVICE_ID "_at_split/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_at_split";
  doc["name"]            = "Forno - Autotune Split Base %";
  doc["state_topic"]     = T_STATE;
  doc["command_topic"]   = T_AT_SPLIT;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:percent";
  doc["unit_of_measurement"] = "%";
  doc["value_template"]  = "{{ value_json.autotune_split }}";
  doc["min"]             = 10;
  doc["max"]             = 90;
  doc["step"]            = 5;
  doc["mode"]            = "slider";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Autotune status (sensor) ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/sensor/" MQTT_DEVICE_ID "_at_status/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_at_status";
  doc["name"]            = "Forno - Autotune Stato";
  doc["state_topic"]     = T_STATE;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:progress-clock";
  doc["value_template"]  = "{{ value_json.autotune_status }}";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  // ---- Autotune cicli/progresso ----
  doc.clear();
  snprintf(topic, sizeof(topic), HA_DISC "/sensor/" MQTT_DEVICE_ID "_at_cycles/config");
  doc["unique_id"]       = MQTT_DEVICE_ID "_at_cycles";
  doc["name"]            = "Forno - Autotune Cicli";
  doc["state_topic"]     = T_STATE;
  doc["availability_topic"] = T_AVAIL;
  doc["icon"]            = "mdi:counter";
  doc["value_template"]  = "{{ value_json.autotune_cycles }}";
  { JsonObject dev = doc.createNestedObject("device"); add_device(dev); }
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic, payload, true);

  Serial.println("[MQTT] Discovery pubblicato — entità registrate in HA");
}

// ================================================================
//  CALLBACK MESSAGGI IN ARRIVO DA HA
// ================================================================
static void mqtt_callback(char* topic_in, byte* payload_in, unsigned int len) {
  // Copia payload in stringa null-terminated
  char msg[32] = {0};
  if (len >= sizeof(msg)) len = sizeof(msg) - 1;
  memcpy(msg, payload_in, len);

  Serial.printf("[MQTT] RX  %s = %s\n", topic_in, msg);

  // Setpoint base
  if (strcmp(topic_in, T_SET_BASE) == 0) {
    float val = atof(msg);
    if (val >= 50.0f && val <= 450.0f) {
      if (MUTEX_TAKE_MS(20)) {
        g_state.set_base  = (double)val;
        g_state.nvs_dirty = true;
        xSemaphoreGive(g_mutex);
      }
      Serial.printf("[MQTT] set_base = %.0f\n", val);
    }
    return;
  }

  // Setpoint cielo
  if (strcmp(topic_in, T_SET_CIELO) == 0) {
    float val = atof(msg);
    if (val >= 50.0f && val <= 450.0f) {
      if (MUTEX_TAKE_MS(20)) {
        g_state.set_cielo = (double)val;
        g_state.nvs_dirty = true;
        xSemaphoreGive(g_mutex);
      }
      Serial.printf("[MQTT] set_cielo = %.0f\n", val);
    }
    return;
  }

  // Comando resistenza BASE
  if (strcmp(topic_in, T_CMD_BASE) == 0) {
    bool on = (strcmp(msg, "ON") == 0);
    // Safety: non accendere se shutdown attivo
    if (on && g_state.safety_shutdown) {
      Serial.println("[MQTT] CMD BASE ignorato: safety shutdown attivo");
      return;
    }
    if (MUTEX_TAKE_MS(20)) {
      g_state.base_enabled = on;
      xSemaphoreGive(g_mutex);
    }
    Serial.printf("[MQTT] base_enabled = %s\n", on ? "ON" : "OFF");
    return;
  }

  // Comando resistenza CIELO
  if (strcmp(topic_in, T_CMD_CIELO) == 0) {
    bool on = (strcmp(msg, "ON") == 0);
    if (on && g_state.safety_shutdown) {
      Serial.println("[MQTT] CMD CIELO ignorato: safety shutdown attivo");
      return;
    }
    if (MUTEX_TAKE_MS(20)) {
      g_state.cielo_enabled = on;
      xSemaphoreGive(g_mutex);
    }
    Serial.printf("[MQTT] cielo_enabled = %s\n", on ? "ON" : "OFF");
    return;
  }

  // Autotune START / STOP
  if (strcmp(topic_in, T_AT_CMD) == 0) {
    if (strcmp(msg, "START") == 0) {
      if (g_state.safety_shutdown) {
        Serial.println("[MQTT] Autotune ignorato: safety shutdown attivo");
        return;
      }
      if (!autotune_is_running()) autotune_start();
    } else if (strcmp(msg, "STOP") == 0) {
      if (autotune_is_running()) autotune_stop();
    }
    return;
  }

  // Autotune split (solo se autotune NON è in corso)
  if (strcmp(topic_in, T_AT_SPLIT) == 0) {
    if (autotune_is_running()) {
      Serial.println("[MQTT] Split ignorato: autotune in corso");
      return;
    }
    int val = atoi(msg);
    if (val >= 10 && val <= 90) {
      if (MUTEX_TAKE_MS(20)) {
        g_state.autotune_split = val;
        xSemaphoreGive(g_mutex);
      }
      Serial.printf("[MQTT] autotune_split = %d%%/%d%%\n", val, 100 - val);
    }
    return;
  }

  // Comando LUCE
  if (strcmp(topic_in, T_CMD_LUCE) == 0) {
    bool on = (strcmp(msg, "ON") == 0);
    if (MUTEX_TAKE_MS(20)) {
      g_state.luce_on = on;
      xSemaphoreGive(g_mutex);
    }
    return;
  }
}

// ================================================================
//  CONNESSIONE / RICONNESSIONE MQTT
// ================================================================
static bool mqtt_connect() {
  char client_id[32];
  snprintf(client_id, sizeof(client_id), "%s_%04X", MQTT_DEVICE_ID,
           (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

  Serial.printf("[MQTT] Connessione a %s:%d ...\n", MQTT_HOST, MQTT_PORT);

  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = mqtt.connect(client_id, MQTT_USER, MQTT_PASS,
                      T_AVAIL, 1, true, "offline");
  } else {
    ok = mqtt.connect(client_id, nullptr, nullptr,
                      T_AVAIL, 1, true, "offline");
  }

  if (!ok) {
    Serial.printf("[MQTT] Fallito (rc=%d)\n", mqtt.state());
    return false;
  }

  Serial.println("[MQTT] Connesso!");

  // Availability online
  mqtt.publish(T_AVAIL, "online", true);

  // Subscribe ai topic di comando
  mqtt.subscribe(T_SET_BASE);
  mqtt.subscribe(T_SET_CIELO);
  mqtt.subscribe(T_CMD_BASE);
  mqtt.subscribe(T_CMD_CIELO);
  mqtt.subscribe(T_CMD_LUCE);
  mqtt.subscribe(T_AT_CMD);
  mqtt.subscribe(T_AT_SPLIT);

  // Registra entità in HA
  publish_discovery();

  g_mqtt_connected = true;
  return true;
}

// ================================================================
//  PUBLISH STATO
// ================================================================
static void publish_state() {
  // Copia stato sotto mutex
  double   temp_base, temp_cielo, set_base, set_cielo;
  double   pid_base, pid_cielo;
  bool     base_en, cielo_en, luce_on, fan_on, relay_b, relay_c;
  bool     tc_err_b, tc_err_c, shutdown;
  SafetyReason reason;
  SensorMode   mode;
  int      pct_base, pct_cielo;

  if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
  temp_base  = g_state.temp_base;
  temp_cielo = g_state.temp_cielo;
  set_base   = g_state.set_base;
  set_cielo  = g_state.set_cielo;
  pid_base   = g_state.pid_out_base;
  pid_cielo  = g_state.pid_out_cielo;
  base_en    = g_state.base_enabled;
  cielo_en   = g_state.cielo_enabled;
  luce_on    = g_state.luce_on;
  fan_on     = g_state.fan_on;
  relay_b    = g_state.relay_base;
  relay_c    = g_state.relay_cielo;
  tc_err_b   = g_state.tc_base_err;
  tc_err_c   = g_state.tc_cielo_err;
  shutdown   = g_state.safety_shutdown;
  reason     = g_state.safety_reason;
  mode       = g_state.sensor_mode;
  pct_base   = g_state.pct_base;
  pct_cielo  = g_state.pct_cielo;
  xSemaphoreGive(g_mutex);

  // Costruisci JSON
  StaticJsonDocument<512> doc;
  doc["temp_base"]  = round(temp_base  * 10.0) / 10.0;
  doc["temp_cielo"] = round(temp_cielo * 10.0) / 10.0;
  doc["set_base"]   = (int)set_base;
  doc["set_cielo"]  = (int)set_cielo;
  doc["pid_base"]   = (int)pid_base;
  doc["pid_cielo"]  = (int)pid_cielo;
  doc["base_en"]    = base_en;
  doc["cielo_en"]   = cielo_en;
  doc["luce"]       = luce_on;
  doc["fan"]        = fan_on;
  doc["relay_base"] = relay_b;
  doc["relay_cielo"]= relay_c;
  doc["tc_err_b"]   = tc_err_b;
  doc["tc_err_c"]   = tc_err_c;
  doc["shutdown"]   = shutdown;
  doc["mode"]       = (mode == SensorMode::SINGLE) ? "SINGLE" : "DUAL";
  doc["pct_base"]   = pct_base;
  doc["pct_cielo"]  = pct_cielo;

  // Motivo allarme come stringa leggibile
  const char* reasons[] = {
    "OK", "TC_ERROR", "OVERTEMP", "RUNAWAY_DOWN", "RUNAWAY_UP", "WDG_TIMEOUT"
  };
  int ri = (int)reason;
  doc["safety_reason"] = (ri >= 0 && ri <= 5) ? reasons[ri] : "UNKNOWN";

  // Autotune
  const char* at_states[] = {"IDLE","RUNNING","DONE","ABORTED"};
  int ati = (int)g_state.autotune_status;
  doc["autotune_running"] = autotune_is_running();
  doc["autotune_status"]  = (ati >= 0 && ati <= 3) ? at_states[ati] : "UNKNOWN";
  doc["autotune_split"]   = g_state.autotune_split;
  doc["autotune_cycles"]  = g_state.autotune_cycles;
  if (g_state.autotune_kp > 0) {
    doc["autotune_kp"]    = serialized(String(g_state.autotune_kp, 3));
    doc["autotune_ki"]    = serialized(String(g_state.autotune_ki, 4));
    doc["autotune_kd"]    = serialized(String(g_state.autotune_kd, 3));
  }

  char payload[512];
  serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(T_STATE, payload, false);
}

// ================================================================
//  TASK_WIFI — Core 0, priorità 1
// ================================================================
void Task_WiFi(void* param) {
  Serial.printf("[Core %d] Task_WiFi avviato\n", xPortGetCoreID());

  uint32_t last_publish_ms  = 0;
  uint32_t last_wifi_try_ms = 0;
  uint32_t last_mqtt_try_ms = 0;

  // Configura MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqtt_callback);
  mqtt.setKeepAlive(MQTT_KEEPALIVE);
  mqtt.setBufferSize(640);   // Discovery + autotune payload

  for (;;) {
    uint32_t now = millis();

    // ── v19: Scan WiFi richiesto dall'UI ──
    if (g_wifi_scan_request) {
      g_wifi_scan_request = false;
      g_wifi_net_count    = -1;   // segnala "scansione in corso"
      Serial.println("[WiFi] Avvio scansione reti...");
      int n = WiFi.scanNetworks(false, true);   // sincrono, include hidden
      int count = (n > WIFI_SCAN_MAX) ? WIFI_SCAN_MAX : n;
      for (int i = 0; i < count; i++) {
        strncpy(g_wifi_nets[i].ssid, WiFi.SSID(i).c_str(), 32);
        g_wifi_nets[i].ssid[32] = '\0';
        g_wifi_nets[i].rssi   = WiFi.RSSI(i);
        g_wifi_nets[i].secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      }
      g_wifi_net_count = count;
      WiFi.scanDelete();
      Serial.printf("[WiFi] Trovate %d reti\n", count);
    }

    // ── v19: Connessione con nuove credenziali ──
    if (s_connect_request) {
      s_connect_request = false;
      WiFi.disconnect(false);
      vTaskDelay(pdMS_TO_TICKS(400));
      const char* use_ssid = strlen(s_dyn_ssid) ? s_dyn_ssid : WIFI_SSID;
      const char* use_pass = strlen(s_dyn_ssid) ? s_dyn_pass : WIFI_PASS;
      Serial.printf("[WiFi] Connessione a %s (richiesta UI)...\n", use_ssid);
      WiFi.begin(use_ssid, use_pass);
      last_wifi_try_ms = 0;   // forza retry immediato
    }

    // ── WiFi ──
    if (WiFi.status() != WL_CONNECTED) {
      g_wifi_connected = false;
      g_mqtt_connected = false;
      if (now - last_wifi_try_ms >= WIFI_RETRY_MS) {
        last_wifi_try_ms = now;
        const char* use_ssid = strlen(s_dyn_ssid) ? s_dyn_ssid : WIFI_SSID;
        const char* use_pass = strlen(s_dyn_ssid) ? s_dyn_pass : WIFI_PASS;
        Serial.printf("[WiFi] Connessione a %s...\n", use_ssid);
        WiFi.begin(use_ssid, use_pass);
        // Attendi fino a 8s
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000)
          vTaskDelay(pdMS_TO_TICKS(200));
        if (WiFi.status() == WL_CONNECTED) {
          g_wifi_connected = true;
          Serial.printf("[WiFi] Connesso! IP: %s\n",
                        WiFi.localIP().toString().c_str());
        } else {
          Serial.println("[WiFi] Timeout");
        }
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    g_wifi_connected = true;

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

    // ── Loop MQTT (processa messaggi in arrivo) ──
    mqtt.loop();

    // ── Publish stato periodico ──
    if (now - last_publish_ms >= MQTT_PUBLISH_MS) {
      last_publish_ms = now;
      publish_state();
    }

    // ── Notifica immediata se safety shutdown scatta ──
    static bool last_shutdown = false;
    if (g_state.safety_shutdown && !last_shutdown) {
      publish_state();  // pubblica subito senza aspettare il ciclo
      last_shutdown = true;
    }
    if (!g_state.safety_shutdown) last_shutdown = false;

    vTaskDelay(pdMS_TO_TICKS(50));  // 20Hz loop MQTT
  }
}

// ================================================================
//  wifi_mqtt_init — chiama da setup()
// ================================================================
void wifi_mqtt_init() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());

  // v19: Carica credenziali salvate dall'UI (se presenti)
  Preferences prefs;
  prefs.begin("wifi_cred", true);
  String saved_ssid = prefs.getString("ssid", "");
  String saved_pass = prefs.getString("pass", "");
  prefs.end();
  if (saved_ssid.length() > 0) {
    strncpy(s_dyn_ssid, saved_ssid.c_str(), 32);
    strncpy(s_dyn_pass, saved_pass.c_str(), 63);
    Serial.printf("[WiFi] Credenziali NVS caricate: %s\n", s_dyn_ssid);
  }
}
