/**
 * ota_manager.h — Forno Pizza v19
 * ================================================================
 * Aggiornamento firmware OTA via HTTP (URL arbitrario .bin)
 *
 * Utilizza:
 *   - Update (built-in ESP32 Arduino core)
 *   - HTTPClient (built-in ESP32 Arduino core)
 *
 * Il download avviene in Task_OTA (core 0, priorità 2) per non
 * bloccare LVGL.  Il progresso è scritto in g_ota_progress (0..100).
 * In caso di errore g_ota_progress viene impostato a -1 e il
 * messaggio di errore in g_ota_status_msg[].
 *
 * UTILIZZO:
 *   1. Imposta g_ota_url[]            (URL del .bin)
 *   2. Imposta g_ota_start_request = true
 *   3. Il task controlla il flag e avvia il download
 *   4. Leggi g_ota_progress e g_ota_status_msg dal loop principale
 *
 * Al completamento (g_ota_progress == 100) il dispositivo si
 * riavvia automaticamente dopo 2 secondi.
 * ================================================================
 */

#pragma once

#include <Arduino.h>

// ----------------------------------------------------------------
//  API
// ----------------------------------------------------------------
void ota_manager_init(void);   // chiama da setup() — crea Task_OTA
void Task_OTA(void* param);    // task FreeRTOS

// Nota: le variabili di stato sono dichiarate in ui_wifi.h:
//   volatile int  g_ota_progress        // 0..100, -1=errore
//   volatile bool g_ota_running         // true durante download
//   volatile bool g_ota_start_request   // flag richiesta avvio
//   char          g_ota_url[256]        // URL firmware
//   char          g_ota_status_msg[64]  // messaggio stato

// ----------------------------------------------------------------
//  TASK
// ----------------------------------------------------------------
#define TASK_OTA_CORE    0
#define TASK_OTA_PRIO    2
#define TASK_OTA_STACK   8192
