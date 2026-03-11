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
 *   Debug display/touch  → TASK_PID=0, TASK_WDG=0, TASK_WIFI=0, LOG_LVGL=1
 *   Debug PID/relay      → TASK_WIFI=0, TASK_LVGL=0 (solo seriale), LOG_PID=1
 *   Debug WiFi/MQTT      → TASK_PID=0, TASK_WDG=0, LOG_WIFI=1
 *   Produzione           → tutto 1, tutti LOG a 0 tranne LOG_SAFETY
 * ================================================================
 */

#pragma once

// ================================================================
//  MODALITÀ DISPLAY — scegli in base al tuo hardware
//
//  DISPLAY_MODE_RGB  1  → Bus_RGB + Panel_RGB (RGB parallelo 16-bit)
//                         Usa questa se ottieni errori Panel_ST7701S/ST7701
//                         Compatibile con tutte le versioni di LovyanGFX
//
//  DISPLAY_MODE_RGB  0  → Bus_SPI + Panel_ST7701S (SPI 4-wire)
//                         Richiede LovyanGFX >= 1.1.7
//
//  JC4827W543C originale usa RGB parallelo → default = 1
// ================================================================
#define DISPLAY_MODE_RGB  1

// ================================================================
//  TASK ENABLE — commenta o metti a 0 per disabilitare il task
// ================================================================

// Task PID: lettura TC, calcolo PID, relay duty cycle
// Disabilitare per debug display senza resistenze collegate
#define TASK_PID_ENABLE       0

// Task Watchdog: supervisione heartbeat Task_PID
// ATTENZIONE: disabilitare solo in debug — in produzione DEVE essere 1
// Se TASK_PID_ENABLE=0 questo deve essere 0 (altrimenti WDG timeout immediato)
#define TASK_WDG_ENABLE       0

// Task LVGL: rendering UI, refresh schermate
// Disabilitare per debug PID puro via seriale
#define TASK_LVGL_ENABLE      1

// Task WiFi/MQTT: connessione, publish stato, ricezione comandi HA
// Disabilitare per debug senza rete o per misurare impatto CPU
#define TASK_WIFI_ENABLE      0

// ================================================================
//  FEATURE ENABLE
// ================================================================

// Schermata splash all'avvio con barra progresso
#define FEATURE_SPLASH        1

// Safety system: overtemp, thermal runaway UP/DOWN
// Disabilitare SOLO per test con resistori di carico, mai in produzione
#define FEATURE_SAFETY        0

// Autotune PID via relay method (Brett Beauregard)
#define FEATURE_AUTOTUNE      0

// OTA via web browser (richiede TASK_WIFI_ENABLE=1)
#define FEATURE_OTA           0

// ================================================================
//  LOG SERIALE — livelli per modulo
//
//  LOG_LEVEL_NONE  0   nessun output
//  LOG_LEVEL_ERROR 1   solo errori fatali
//  LOG_LEVEL_WARN  2   errori + avvisi
//  LOG_LEVEL_INFO  3   info operative (default produzione)
//  LOG_LEVEL_DEBUG 4   dettaglio completo (verbose)
// ================================================================
#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

// Log di sistema / avvio / memoria
#define LOG_SYSTEM        LOG_LEVEL_DEBUG

// Log Task_PID: temperatura, relay, PID output ogni ciclo
// ATTENZIONE: LOG_LEVEL_DEBUG stampa ogni 500ms → può saturare il seriale
#define LOG_PID           LOG_LEVEL_INFO

// Log Task_Watchdog: heartbeat check
#define LOG_WDG           LOG_LEVEL_WARN

// Log Task_LVGL: refresh schermata, transizioni
#define LOG_LVGL          LOG_LEVEL_WARN

// Log WiFi/MQTT: connessione, publish, ricezione comandi
#define LOG_WIFI          LOG_LEVEL_INFO

// Log Safety: shutdown, overtemp, runaway — sempre almeno WARN
#define LOG_SAFETY        LOG_LEVEL_WARN

// Log memoria PSRAM/SRAM (stampato in display_init)
#define LOG_MEMORY        LOG_LEVEL_INFO

// ================================================================
//  MACRO DI LOG — uso interno, non modificare
//  Esempio: LOG_I(LOG_PID, "[PID] temp=%.1f\n", temp);
//           LOG_D(LOG_LVGL, "[LVGL] refresh screen %d\n", scr);
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
//  SANITY CHECK — combinazioni non valide
// ================================================================
#if TASK_WDG_ENABLE && !TASK_PID_ENABLE
  #warning "TASK_WDG_ENABLE=1 con TASK_PID_ENABLE=0: il watchdog farà timeout immediato!"
  #warning "→ Imposta TASK_WDG_ENABLE=0 quando TASK_PID_ENABLE=0"
#endif

#if FEATURE_OTA && !TASK_WIFI_ENABLE
  #warning "FEATURE_OTA=1 richiede TASK_WIFI_ENABLE=1 — OTA disabilitato in pratica"
#endif

#if !FEATURE_SAFETY
  #warning "FEATURE_SAFETY=0: sistema di sicurezza disabilitato — solo per test!"
#endif
