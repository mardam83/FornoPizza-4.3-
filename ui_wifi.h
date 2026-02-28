/**
 * ui_wifi.h — Forno Pizza v19
 * ================================================================
 * Due nuove schermate:
 *
 *  WIFI_SCAN  — scansione reti WiFi disponibili
 *    y=0..25    header blu "📶 RETE WiFi"
 *    y=26..193  lista reti (scroll interno, max 8 voci)
 *               ogni riga: [RSSI icon] [SSID] [🔒 se protetta]
 *    y=194..213 pulsanti [AGGIORNA] [◀ INDIETRO]
 *    y=214..239 status bar connessione
 *
 *  WIFI_PWD   — inserimento password (tastiera LVGL)
 *    y=0..25    header con SSID selezionato
 *    y=26..70   campo testo password + occhio
 *    y=71..155  tastiera LVGL compatta
 *    y=156..185 [CONNETTI] [ANNULLA]
 *    y=186..239 status connessione
 *
 *  OTA        — aggiornamento firmware via URL HTTP
 *    y=0..25    header arancione "⬆ AGGIORNAMENTO OTA"
 *    y=26..75   campo URL server (es. http://192.168.1.x/firmware.bin)
 *    y=76..85   tastiera URL (usa pannello LVGL keyboard)
 *    y=86..155  barra avanzamento + etichetta %
 *    y=156..185 [AVVIA OTA] [◀ INDIETRO]
 *    y=186..239 status OTA
 * ================================================================
 */

#pragma once
#include <lvgl.h>

// ----------------------------------------------------------------
//  WIDGET — WIFI_SCAN
// ----------------------------------------------------------------
extern lv_obj_t* ui_ScreenWifi;
extern lv_obj_t* ui_WifiList;          // lv_list — reti trovate
extern lv_obj_t* ui_WifiStatusLbl;     // connessione corrente
extern lv_obj_t* ui_BtnWifiScan;       // pulsante aggiorna
extern lv_obj_t* ui_BtnWifiBack;       // pulsante indietro

// ----------------------------------------------------------------
//  WIDGET — WIFI_PWD
// ----------------------------------------------------------------
extern lv_obj_t* ui_ScreenWifiPwd;
extern lv_obj_t* ui_WifiPwdSsidLbl;   // titolo header con SSID
extern lv_obj_t* ui_WifiPwdTA;        // textarea password
extern lv_obj_t* ui_WifiKbd;          // lv_keyboard
extern lv_obj_t* ui_BtnWifiConnect;   // pulsante connetti
extern lv_obj_t* ui_BtnWifiCancel;    // pulsante annulla
extern lv_obj_t* ui_WifiPwdStatusLbl; // feedback connessione

// ----------------------------------------------------------------
//  WIDGET — OTA
// ----------------------------------------------------------------
extern lv_obj_t* ui_ScreenOta;
extern lv_obj_t* ui_OtaUrlTA;          // textarea URL
extern lv_obj_t* ui_OtaKbd;            // lv_keyboard URL
extern lv_obj_t* ui_OtaBar;            // barra progresso
extern lv_obj_t* ui_OtaBarLbl;         // "0 %"
extern lv_obj_t* ui_BtnOtaStart;       // pulsante avvia
extern lv_obj_t* ui_BtnOtaBack;        // pulsante indietro
extern lv_obj_t* ui_OtaStatusLbl;      // feedback OTA

// ----------------------------------------------------------------
//  STATO WIFI SCAN (thread-safe via flag atomici)
// ----------------------------------------------------------------
#define WIFI_SCAN_MAX  12              // max reti mostrate

struct WifiNetInfo {
    char    ssid[33];
    int32_t rssi;
    bool    secure;    // richiede password
};

extern WifiNetInfo  g_wifi_nets[WIFI_SCAN_MAX];
extern volatile int g_wifi_net_count;   // -1=scansione in corso, 0..N=risultati
extern char         g_wifi_selected_ssid[33];
extern volatile bool g_wifi_scan_request;  // flag: richiedi nuova scansione

// ----------------------------------------------------------------
//  STATO OTA
// ----------------------------------------------------------------
extern volatile int  g_ota_progress;       // 0..100, -1=errore
extern volatile bool g_ota_running;
extern volatile bool g_ota_start_request;
extern char          g_ota_url[256];
extern char          g_ota_status_msg[64];

// ----------------------------------------------------------------
//  FUNZIONI
// ----------------------------------------------------------------
void ui_build_wifi(void);           // chiamata da ui_init()
void ui_build_ota(void);            // chiamata da ui_init()

void ui_show_wifi_scan(void);       // naviga a WIFI_SCAN
void ui_show_wifi_pwd(const char* ssid); // naviga a WIFI_PWD
void ui_show_ota(void);             // naviga a OTA

// Chiamate dal task WiFi (core 0) quando lo stato cambia
void ui_wifi_update_list(void);     // rinnova la lista reti (da loop LVGL)
void ui_wifi_update_status(void);   // aggiorna label stato connessione
void ui_ota_update_progress(void);  // aggiorna barra OTA
