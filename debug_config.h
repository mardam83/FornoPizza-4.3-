/**
 * debug_config.h — Forno Pizza S3 — Configurazione Debug & Task
 * ================================================================
 * Modifica questo file per controllare:
 *   - Quali task FreeRTOS vengono avviati
 *   - Livello di verbosità del log seriale
 *   - Feature opzionali (WiFi, OTA, Autotune, Safety)
 *
 * REGOLA: definire a 1 = abilitato, 0 = disabilitato
 *
 * WORKFLOW TIPICO:
 *   Debug display/touch  → TASK_PID=0, TASK_WDG=0, TASK_WIFI=0
 *                          LOG_LVGL=LOG_LEVEL_DEBUG
 *   Debug PID/relay      → TASK_WIFI=0, LOG_PID=LOG_LEVEL_DEBUG
 *   Produzione           → tutto 1, LOG a INFO tranne LOG_SAFETY
 * ================================================================
 */

#pragma once

// ================================================================
//  TASK ENABLE
// ================================================================

// Task PID: lettura TC, calcolo PID, relay duty cycle
#define TASK_PID_ENABLE       0   // [PROD] 1

// Task Watchdog: supervisione heartbeat Task_PID
// ⚠ DEVE essere 0 se TASK_PID_ENABLE=0 (altrimenti WDG timeout immediato)
#define TASK_WDG_ENABLE       0   // [PROD] 1

// Task LVGL: rendering UI, refresh schermate
#define TASK_LVGL_ENABLE      1

// Task WiFi/MQTT
#define TASK_WIFI_ENABLE      1   // [PROD] 1

// ================================================================
//  FEATURE ENABLE
// ================================================================
#define FEATURE_SPLASH        1
#define FEATURE_SAFETY        0   // [PROD] 1
#define FEATURE_AUTOTUNE      0
#define FEATURE_OTA           0

// ================================================================
//  LOG SERIALE — livelli per modulo
//
//  LOG_LEVEL_NONE  0   nessun output
//  LOG_LEVEL_ERROR 1   solo errori fatali
//  LOG_LEVEL_WARN  2   errori + avvisi
//  LOG_LEVEL_INFO  3   info operative
//  LOG_LEVEL_DEBUG 4   dettaglio completo (verbose)
// ================================================================
#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

// Log di sistema / avvio / memoria
#define LOG_SYSTEM        LOG_LEVEL_DEBUG

// Log Task_PID
#define LOG_PID           LOG_LEVEL_DEBUG   // [PROD] LOG_LEVEL_INFO

// Log Task_Watchdog
#define LOG_WDG           LOG_LEVEL_DEBUG   // [PROD] LOG_LEVEL_WARN

// Log Task_LVGL — era WARN in repo, non stampava nulla di utile
#define LOG_LVGL          LOG_LEVEL_DEBUG   // [PROD] LOG_LEVEL_WARN

// Log WiFi/MQTT
#define LOG_WIFI          LOG_LEVEL_DEBUG   // [PROD] LOG_LEVEL_INFO

// Log Safety — sempre almeno WARN
#define LOG_SAFETY        LOG_LEVEL_DEBUG   // [PROD] LOG_LEVEL_WARN

// Log memoria PSRAM/SRAM
#define LOG_MEMORY        LOG_LEVEL_DEBUG   // [PROD] LOG_LEVEL_INFO

// ================================================================
//  MACRO DI LOG
// ================================================================
#define LOG_E(module, fmt, ...) \
  do { if ((module) >= LOG_LEVEL_ERROR) Serial.printf("[ERR] " fmt, ##__VA_ARGS__); } while(0)

#define LOG_W(module, fmt, ...) \
  do { if ((module) >= LOG_LEVEL_WARN)  Serial.printf("[WRN] " fmt, ##__VA_ARGS__); } while(0)

#define LOG_I(module, fmt, ...) \
  do { if ((module) >= LOG_LEVEL_INFO)  Serial.printf(fmt, ##__VA_ARGS__); } while(0)

#define LOG_D(module, fmt, ...) \
  do { if ((module) >= LOG_LEVEL_DEBUG) Serial.printf("[DBG] " fmt, ##__VA_ARGS__); } while(0)

// ================================================================
//  SANITY CHECK
// ================================================================
#if TASK_WDG_ENABLE && !TASK_PID_ENABLE
  #warning "TASK_WDG_ENABLE=1 con TASK_PID_ENABLE=0: watchdog timeout immediato!"
  #warning "→ Imposta TASK_WDG_ENABLE=0 quando TASK_PID_ENABLE=0"
#endif

#if FEATURE_OTA && !TASK_WIFI_ENABLE
  #warning "FEATURE_OTA=1 richiede TASK_WIFI_ENABLE=1 — OTA disabilitato in pratica"
#endif

#if !FEATURE_SAFETY
  #warning "FEATURE_SAFETY=0: sistema di sicurezza disabilitato — solo per test!"
#endif
