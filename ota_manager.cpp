/**
 * ota_manager.cpp — Forno Pizza v19
 * ================================================================
 * Download OTA via HTTP con HTTPClient + Update
 *
 * Librerie built-in ESP32 (nessuna installazione richiesta):
 *   #include <HTTPClient.h>
 *   #include <Update.h>
 * ================================================================
 */

#include "ota_manager.h"
#include "ui_wifi.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>

// ================================================================
//  Task_OTA — esegue su core 0
// ================================================================
void Task_OTA(void* param) {
    for (;;) {
        // Attendi richiesta
        if (!g_ota_start_request) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Consuma il flag
        g_ota_start_request = false;
        g_ota_running       = true;
        g_ota_progress      = 0;

        // --------------------------------------------------------
        //  Verifica connessione WiFi
        // --------------------------------------------------------
        if (WiFi.status() != WL_CONNECTED) {
            g_ota_progress = -1;
            strncpy(g_ota_status_msg, "Errore: WiFi non connesso",
                    sizeof(g_ota_status_msg));
            g_ota_running = false;
            continue;
        }

        // --------------------------------------------------------
        //  Apri connessione HTTP
        // --------------------------------------------------------
        HTTPClient http;
        http.setTimeout(15000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        if (!http.begin(g_ota_url)) {
            g_ota_progress = -1;
            strncpy(g_ota_status_msg, "Errore: URL non valido",
                    sizeof(g_ota_status_msg));
            g_ota_running = false;
            continue;
        }

        snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                 "Connessione a server...");

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                     "Errore HTTP: %d", httpCode);
            g_ota_progress = -1;
            http.end();
            g_ota_running = false;
            continue;
        }

        int contentLen = http.getSize();
        if (contentLen <= 0) {
            strncpy(g_ota_status_msg, "Errore: dimensione sconosciuta",
                    sizeof(g_ota_status_msg));
            g_ota_progress = -1;
            http.end();
            g_ota_running = false;
            continue;
        }

        // --------------------------------------------------------
        //  Inizia Update
        // --------------------------------------------------------
        if (!Update.begin(contentLen, U_FLASH)) {
            snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                     "Errore Update.begin: %s", Update.errorString());
            g_ota_progress = -1;
            http.end();
            g_ota_running = false;
            continue;
        }

        snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                 "Download: %d KB", contentLen / 1024);

        // --------------------------------------------------------
        //  Stream download con progress
        // --------------------------------------------------------
        WiFiClient* stream = http.getStreamPtr();
        uint8_t buf[1024];
        int written = 0;
        int retries = 0;

        while (written < contentLen && http.connected()) {
            size_t avail = stream->available();
            if (!avail) {
                vTaskDelay(pdMS_TO_TICKS(1));
                retries++;
                if (retries > 10000) {  // ~10 secondi timeout
                    break;
                }
                continue;
            }
            retries = 0;

            size_t toRead = min(avail, sizeof(buf));
            int n = stream->readBytes(buf, toRead);
            if (n > 0) {
                size_t wr = Update.write(buf, n);
                if (wr != (size_t)n) {
                    snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                             "Errore scrittura flash: %s", Update.errorString());
                    g_ota_progress = -1;
                    break;
                }
                written += n;
                // Aggiorna progresso 0..99 durante download
                g_ota_progress = (written * 99) / contentLen;
                snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                         "Download: %d / %d KB",
                         written / 1024, contentLen / 1024);
            }
            vTaskDelay(pdMS_TO_TICKS(1));  // yield
        }

        http.end();

        if (g_ota_progress < 0) {
            Update.abort();
            g_ota_running = false;
            continue;
        }

        // --------------------------------------------------------
        //  Finalizza flash
        // --------------------------------------------------------
        if (!Update.end(true)) {
            snprintf(g_ota_status_msg, sizeof(g_ota_status_msg),
                     "Errore fine OTA: %s", Update.errorString());
            g_ota_progress = -1;
            g_ota_running  = false;
            continue;
        }

        // --------------------------------------------------------
        //  Successo → riavvia dopo 2 secondi
        // --------------------------------------------------------
        g_ota_progress = 100;
        strncpy(g_ota_status_msg, "Aggiornamento completato! Riavvio...",
                sizeof(g_ota_status_msg));
        g_ota_running = false;

        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP.restart();
    }
}

// ================================================================
//  ota_manager_init
// ================================================================
void ota_manager_init(void) {
    xTaskCreatePinnedToCore(
        Task_OTA,
        "Task_OTA",
        TASK_OTA_STACK,
        NULL,
        TASK_OTA_PRIO,
        NULL,
        TASK_OTA_CORE
    );
}
