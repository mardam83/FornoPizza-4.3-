/**
 * debug_config.h — Forno Pizza S3 — Configurazione Debug & Task
 * ================================================================
 * SIMULATOR_MODE=1 → nessun hardware reale richiesto.
 * Solo display collegato. Tutto il resto è virtuale.
 *
 * SEQUENZA TEST AUTOMATICA (vedi simulator.h):
 *   FASE 1 — Riscaldamento PID normale fino a setpoint
 *   FASE 2 — Test OVERTEMP: temperatura forzata a 490°C → SHUTDOWN
 *   FASE 3 — Reset, test TC_ERROR: sensore forzato NAN → SHUTDOWN
 *   FASE 4 — Reset, test Autotune relay oscillation → Kp/Ki/Kd
 *   FASE 5 — Riscaldamento finale con parametri autotune
 *
 * Ogni fase produce log dettagliato su Serial con prefisso [TEST].
 * ================================================================
 */

#pragma once

// ================================================================
//  SIMULATORE HARDWARE
// ================================================================
#define SIMULATOR_MODE        1   // 1=simulatore, 0=hardware reale

// ================================================================
//  TASK ENABLE
// ================================================================
#define TASK_PID_ENABLE       1
#define TASK_WDG_ENABLE       1
#define TASK_LVGL_ENABLE      1
#define TASK_WIFI_ENABLE      0   // non necessario per test HW

// ================================================================
//  FEATURE ENABLE
// ================================================================
#define FEATURE_SPLASH        1
#define FEATURE_SAFETY        1
#define FEATURE_AUTOTUNE      1
#define FEATURE_OTA           0

// ================================================================
//  LOG SERIALE
// ================================================================
#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

#define LOG_SYSTEM        LOG_LEVEL_INFO
#define LOG_PID           LOG_LEVEL_INFO
#define LOG_WDG           LOG_LEVEL_INFO
#define LOG_LVGL          LOG_LEVEL_WARN
#define LOG_WIFI          LOG_LEVEL_NONE
#define LOG_SAFETY        LOG_LEVEL_INFO
#define LOG_MEMORY        LOG_LEVEL_INFO

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
#endif
#if SIMULATOR_MODE && !TASK_PID_ENABLE
  #warning "SIMULATOR_MODE=1 richiede TASK_PID_ENABLE=1"
#endif
