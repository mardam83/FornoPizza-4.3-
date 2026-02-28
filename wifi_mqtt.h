/**
 * wifi_mqtt.h — Forno Pizza Controller v14
 * ================================================================
 * WiFi + MQTT con Home Assistant MQTT Discovery
 *
 * CONFIGURAZIONE — modifica solo questa sezione:
 *   WIFI_SSID / WIFI_PASS   → credenziali rete
 *   MQTT_HOST               → IP o hostname del broker Mosquitto
 *   MQTT_PORT               → default 1883
 *   MQTT_USER / MQTT_PASS   → credenziali MQTT (vuote se non configurate)
 *   MQTT_DEVICE_ID          → ID univoco del device in HA
 *
 * TOPIC PUBBLICATI (forno → HA):
 *   forno/<ID>/state        → JSON completo ogni MQTT_PUBLISH_MS
 *
 * TOPIC SOTTOSCRITTI (HA → forno):
 *   forno/<ID>/set/base     → setpoint base  (es. "280")
 *   forno/<ID>/set/cielo    → setpoint cielo (es. "320")
 *   forno/<ID>/cmd/base     → ON / OFF
 *   forno/<ID>/cmd/cielo    → ON / OFF
 *   forno/<ID>/cmd/luce     → ON / OFF
 *
 * MQTT DISCOVERY:
 *   homeassistant/sensor/.../config      → temperature, PID, allarmi
 *   homeassistant/switch/.../config      → relay base, cielo, luce
 *   homeassistant/number/.../config      → setpoint base, cielo
 *   homeassistant/binary_sensor/.../config → ventola, safety
 * ================================================================
 */

#pragma once

// ================================================================
//  CONFIGURAZIONE UTENTE — modifica qui
// ================================================================
#define WIFI_SSID         "TUO_SSID"
#define WIFI_PASS         "TUA_PASSWORD"

#define MQTT_HOST         "192.168.1.100"   // IP del tuo HA / broker Mosquitto
#define MQTT_PORT         1883
#define MQTT_USER         ""                // lascia vuoto se non richiesto
#define MQTT_PASS         ""

#define MQTT_DEVICE_ID    "forno_pizza"     // ID univoco — cambia se hai più forni
#define MQTT_DEVICE_NAME  "Forno Pizza"

// ================================================================
//  PARAMETRI
// ================================================================
#define MQTT_PUBLISH_MS   2000    // ms tra ogni publish di stato
#define WIFI_RETRY_MS     10000   // ms tra tentativi di riconnessione WiFi
#define MQTT_RETRY_MS     5000    // ms tra tentativi di riconnessione MQTT
#define MQTT_KEEPALIVE    60      // secondi keepalive MQTT

// ================================================================
//  TASK
// ================================================================
#define TASK_WIFI_CORE    0       // stesso core di LVGL, priorità più bassa
#define TASK_WIFI_PRIO    1       // uguale a LVGL — non interferisce con PID
#define TASK_WIFI_STACK   8192    // WiFi+MQTT necessita stack generoso

// ================================================================
//  API
// ================================================================
void wifi_mqtt_init();   // chiama da setup() dopo mutex e NVS
void Task_WiFi(void* param);

// v19: Chiamata dall'UI WiFi Scan per connettersi a una rete scelta
// Salva le credenziali in NVS e richiede riconnessione immediata
void wifi_request_connect(const char* ssid, const char* pass);
