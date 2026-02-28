/**
 * web_ota.h — Forno Pizza v19
 * ================================================================
 * Web server OTA via upload drag&drop dal browser
 *
 * Avvia un server HTTP sulla porta 80 dell'ESP32.
 * Raggiungibile da browser su:
 *   http://<ip-esp32>/
 *   http://<ip-esp32>/update   (alias)
 *
 * Il progresso viene scritto nelle stesse variabili condivise
 * con ota_manager / LVGL:
 *   g_ota_progress    (0..100, -1=errore)
 *   g_ota_running     (true durante flash)
 *   g_ota_status_msg  (messaggio stato)
 *
 * UTILIZZO:
 *   1. Chiama web_ota_init() da setup() DOPO WiFi.begin()
 *   2. Chiama web_ota_handle() nel loop() principale
 *      (oppure avvia Task_WebOTA che lo gestisce autonomamente)
 *
 * Dipendenze (built-in ESP32 Arduino core):
 *   #include <WebServer.h>
 *   #include <Update.h>
 * ================================================================
 */

#pragma once

#include <Arduino.h>

// ----------------------------------------------------------------
//  API pubblica
// ----------------------------------------------------------------
void web_ota_init(void);     // crea il task WebOTA (chiama da setup)
void web_ota_handle(void);   // poll server (usare se non si usa il task)

// ----------------------------------------------------------------
//  Task FreeRTOS interno
// ----------------------------------------------------------------
void Task_WebOTA(void* param);

#define TASK_WEBOTA_CORE   0
#define TASK_WEBOTA_PRIO   1
#define TASK_WEBOTA_STACK  8192

#define WEB_OTA_PORT       80
