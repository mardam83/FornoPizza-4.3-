/**
 * ui_wifi.cpp — Forno Pizza v21-S3
 * ================================================================
 * Schermate adattate per 480×272 (da 320×240):
 *   WIFI_SCAN  → lista reti scrollabile + status
 *   WIFI_PWD   → tastiera LVGL inserimento password
 *   OTA        → URL firmware + barra progresso
 * ================================================================
 */

#include "ui_wifi.h"
#include "ui.h"
#include <lvgl.h>
#include <WiFi.h>

// ================================================================
//  EXTERN — variabili definite in wifi_mqtt.cpp
// ================================================================
extern volatile bool g_wifi_connected;
extern volatile bool g_mqtt_connected;

// ================================================================
//  GLOBALS
// ================================================================
lv_obj_t* ui_ScreenWifi     = NULL;
lv_obj_t* ui_WifiList       = NULL;
lv_obj_t* ui_WifiStatusLbl  = NULL;
lv_obj_t* ui_BtnWifiScan    = NULL;
lv_obj_t* ui_BtnWifiBack    = NULL;

lv_obj_t* ui_ScreenWifiPwd    = NULL;
lv_obj_t* ui_WifiPwdSsidLbl   = NULL;
lv_obj_t* ui_WifiPwdTA        = NULL;
lv_obj_t* ui_WifiKbd          = NULL;
lv_obj_t* ui_BtnWifiConnect   = NULL;
lv_obj_t* ui_BtnWifiCancel    = NULL;
lv_obj_t* ui_WifiPwdStatusLbl = NULL;

lv_obj_t* ui_ScreenOta     = NULL;
lv_obj_t* ui_OtaUrlTA      = NULL;
lv_obj_t* ui_OtaKbd        = NULL;
lv_obj_t* ui_OtaBar        = NULL;
lv_obj_t* ui_OtaBarLbl     = NULL;
lv_obj_t* ui_BtnOtaStart   = NULL;
lv_obj_t* ui_BtnOtaBack    = NULL;
lv_obj_t* ui_OtaStatusLbl  = NULL;

WifiNetInfo  g_wifi_nets[WIFI_SCAN_MAX] = {};
volatile int g_wifi_net_count     = 0;
char         g_wifi_selected_ssid[33] = "";
volatile bool g_wifi_scan_request  = false;

volatile int  g_ota_progress      = 0;
volatile bool g_ota_running       = false;
volatile bool g_ota_start_request = false;
char          g_ota_url[256]      = "";
char          g_ota_status_msg[64]= "Inserisci URL e premi AVVIA";

// ================================================================
//  COLORI LOCALI
// ================================================================
#define COL_WIFI    lv_color_make(0x00, 0x80, 0xFF)
#define COL_OTA     lv_color_make(0xFF, 0x6B, 0x00)
#define COL_OK      lv_color_make(0x00, 0xE6, 0x76)
#define COL_ERR     lv_color_make(0xFF, 0x30, 0x30)
#define COL_DARK    lv_color_make(0x10, 0x10, 0x1A)
#define COL_DGRAY   lv_color_make(0x28, 0x28, 0x38)
#define COL_GRAY    lv_color_make(0x60, 0x60, 0x70)
#define COL_WHITE   lv_color_make(0xFF, 0xFF, 0xFF)
#define COL_YELLOW  lv_color_make(0xFF, 0xE0, 0x00)

// ================================================================
//  FORWARD DECLARATIONS CALLBACKS
// ================================================================
static void cb_wifi_scan_btn(lv_event_t* e);
static void cb_wifi_back_btn(lv_event_t* e);
static void cb_wifi_net_selected(lv_event_t* e);
static void cb_wifi_pwd_kbd(lv_event_t* e);
static void cb_wifi_connect_btn(lv_event_t* e);
static void cb_wifi_cancel_btn(lv_event_t* e);
static void cb_ota_kbd(lv_event_t* e);
static void cb_ota_start_btn(lv_event_t* e);
static void cb_ota_back_btn(lv_event_t* e);

// ================================================================
//  HELPER: bottone azione (bordato, sfondo scuro)
// ================================================================
static lv_obj_t* make_action_btn(lv_obj_t* parent,
                                  int x, int y, int w, int h,
                                  const char* label,
                                  lv_color_t border_col,
                                  lv_event_cb_t cb) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_make(0x1C, 0x1C, 0x2C), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, border_col, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, border_col, 0);
    lv_obj_center(l);
    return b;
}

// Helper label status
static lv_obj_t* make_status_lbl(lv_obj_t* parent, int y, const char* txt) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(l, COL_GRAY, 0);
    lv_obj_set_pos(l, 4, y);
    lv_label_set_long_mode(l, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(l, 472);
    return l;
}

// RSSI → simbolo
static const char* rssi_symbol(int32_t rssi) {
    if (rssi >= -60) return LV_SYMBOL_WIFI;
    if (rssi >= -75) return LV_SYMBOL_WIFI;
    return LV_SYMBOL_CLOSE;
}

// ================================================================
//  BUILD — WIFI_SCAN
// ================================================================
// Layout 480×272:
//   Header      y=0..31   h=32  — titolo centrato
//   Lista reti  y=32..211 h=180 — scrollabile
//   Sep         y=212     h=2
//   Azioni      y=214..237 h=24 — AGGIORNA(234) | INDIETRO(234)
//   Status      y=240..271 h=32 — label connessione
// ================================================================
void ui_build_wifi(void) {
    ui_ScreenWifi = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenWifi, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_ScreenWifi, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_ScreenWifi, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* hdr = lv_obj_create(ui_ScreenWifi);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 480, 32);
    lv_obj_set_style_bg_color(hdr, COL_WIFI, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* hdr_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_lbl, LV_SYMBOL_WIFI "  RETE WiFi");
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(hdr_lbl, lv_color_black(), 0);
    lv_obj_align(hdr_lbl, LV_ALIGN_CENTER, 0, 0);

    // Pannello lista (scrollabile)
    lv_obj_t* list_panel = lv_obj_create(ui_ScreenWifi);
    lv_obj_set_pos(list_panel, 0, 32);
    lv_obj_set_size(list_panel, 480, 180);
    lv_obj_set_style_bg_color(list_panel, lv_color_make(0x14, 0x14, 0x22), 0);
    lv_obj_set_style_bg_opa(list_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list_panel, 0, 0);
    lv_obj_set_style_radius(list_panel, 0, 0);
    lv_obj_set_style_pad_all(list_panel, 2, 0);
    lv_obj_set_flex_flow(list_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Lista reti
    ui_WifiList = lv_list_create(list_panel);
    lv_obj_set_size(ui_WifiList, 476, 176);
    lv_obj_set_style_bg_color(ui_WifiList, lv_color_make(0x14, 0x14, 0x22), 0);
    lv_obj_set_style_bg_opa(ui_WifiList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_WifiList, 0, 0);
    lv_obj_set_style_pad_row(ui_WifiList, 2, 0);
    lv_obj_set_style_pad_all(ui_WifiList, 2, 0);
    lv_list_add_text(ui_WifiList, "Premi AGGIORNA per scansionare...");

    // Separatore y=212
    lv_obj_t* sep = lv_obj_create(ui_ScreenWifi);
    lv_obj_set_pos(sep, 0, 212);
    lv_obj_set_size(sep, 480, 2);
    lv_obj_set_style_bg_color(sep, COL_WIFI, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    // Pulsanti azione y=214 h=26 — larghezza 238 ciascuno
    ui_BtnWifiScan = make_action_btn(ui_ScreenWifi,
        2, 214, 236, 26, LV_SYMBOL_REFRESH " AGGIORNA", COL_WIFI, cb_wifi_scan_btn);
    ui_BtnWifiBack = make_action_btn(ui_ScreenWifi,
        242, 214, 236, 26, LV_SYMBOL_LEFT " INDIETRO", COL_GRAY, cb_wifi_back_btn);

    // Status y=242
    ui_WifiStatusLbl = make_status_lbl(ui_ScreenWifi, 244, "Non connesso");

    // ----------------------------------------------------------------
    //  WIFI_PWD — schermata password (480×272)
    //  Header   y=0..31   h=32
    //  Label    y=34
    //  TextArea y=50..90  h=38 w=472
    //  Tastiera y=92..212 h=120 w=480
    //  Pulsanti y=214..239 h=26
    //  Status   y=244
    // ----------------------------------------------------------------
    ui_ScreenWifiPwd = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenWifiPwd, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_ScreenWifiPwd, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_ScreenWifiPwd, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* phdr = lv_obj_create(ui_ScreenWifiPwd);
    lv_obj_set_pos(phdr, 0, 0);
    lv_obj_set_size(phdr, 480, 32);
    lv_obj_set_style_bg_color(phdr, COL_WIFI, 0);
    lv_obj_set_style_bg_opa(phdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(phdr, 0, 0);
    lv_obj_set_style_radius(phdr, 0, 0);
    lv_obj_set_style_pad_all(phdr, 0, 0);
    lv_obj_clear_flag(phdr, LV_OBJ_FLAG_SCROLLABLE);

    ui_WifiPwdSsidLbl = lv_label_create(phdr);
    lv_label_set_text(ui_WifiPwdSsidLbl, LV_SYMBOL_WIFI "  Password");
    lv_obj_set_style_text_font(ui_WifiPwdSsidLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_WifiPwdSsidLbl, lv_color_black(), 0);
    lv_obj_align(ui_WifiPwdSsidLbl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* pwd_lbl = lv_label_create(ui_ScreenWifiPwd);
    lv_label_set_text(pwd_lbl, "Password:");
    lv_obj_set_style_text_font(pwd_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pwd_lbl, COL_GRAY, 0);
    lv_obj_set_pos(pwd_lbl, 6, 34);

    // TextArea password — larghezza 472
    ui_WifiPwdTA = lv_textarea_create(ui_ScreenWifiPwd);
    lv_obj_set_pos(ui_WifiPwdTA, 4, 50);
    lv_obj_set_size(ui_WifiPwdTA, 472, 38);
    lv_textarea_set_password_mode(ui_WifiPwdTA, true);
    lv_textarea_set_max_length(ui_WifiPwdTA, 63);
    lv_textarea_set_one_line(ui_WifiPwdTA, true);
    lv_textarea_set_placeholder_text(ui_WifiPwdTA, "Inserisci password WiFi...");
    lv_obj_set_style_bg_color(ui_WifiPwdTA, COL_DGRAY, 0);
    lv_obj_set_style_bg_opa(ui_WifiPwdTA, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_WifiPwdTA, COL_WHITE, 0);
    lv_obj_set_style_text_font(ui_WifiPwdTA, &lv_font_montserrat_16, 0);
    lv_obj_set_style_border_color(ui_WifiPwdTA, COL_WIFI, 0);
    lv_obj_set_style_border_width(ui_WifiPwdTA, 2, 0);

    // Tastiera LVGL — y=90 h=122 w=480
    ui_WifiKbd = lv_keyboard_create(ui_ScreenWifiPwd);
    lv_obj_set_pos(ui_WifiKbd, 0, 90);
    lv_obj_set_size(ui_WifiKbd, 480, 122);
    lv_keyboard_set_mode(ui_WifiKbd, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(ui_WifiKbd, ui_WifiPwdTA);
    lv_obj_set_style_bg_color(ui_WifiKbd, lv_color_make(0x1A, 0x1A, 0x2A), 0);
    lv_obj_set_style_bg_opa(ui_WifiKbd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_WifiKbd, COL_WHITE, 0);
    lv_obj_add_event_cb(ui_WifiKbd, cb_wifi_pwd_kbd, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_WifiKbd, cb_wifi_pwd_kbd, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_WifiKbd, cb_wifi_pwd_kbd, LV_EVENT_CANCEL, NULL);

    // Pulsanti y=214 h=26
    ui_BtnWifiConnect = make_action_btn(ui_ScreenWifiPwd,
        2, 214, 236, 26, LV_SYMBOL_OK " CONNETTI", COL_OK, cb_wifi_connect_btn);
    ui_BtnWifiCancel = make_action_btn(ui_ScreenWifiPwd,
        242, 214, 236, 26, LV_SYMBOL_CLOSE " ANNULLA", COL_ERR, cb_wifi_cancel_btn);

    // Status
    ui_WifiPwdStatusLbl = make_status_lbl(ui_ScreenWifiPwd, 244, "");
}

// ================================================================
//  BUILD — OTA (480×272)
//  Header   y=0..31   h=32
//  Label    y=34
//  TextArea y=50..90  h=38 w=472
//  Tastiera y=92..192 h=100 w=480 (SPECIAL mode URL)
//  Bar      y=194..214 h=20 w=380
//  BarLbl   x=386      y=194
//  Sep      y=216      h=2
//  Azioni   y=218..243 h=26 — AVVIA(236)|INDIETRO(236)
//  Status   y=246
// ================================================================
void ui_build_ota(void) {
    ui_ScreenOta = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenOta, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_ScreenOta, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_ScreenOta, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* hdr = lv_obj_create(ui_ScreenOta);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 480, 32);
    lv_obj_set_style_bg_color(hdr, COL_OTA, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* hdr_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_lbl, LV_SYMBOL_UPLOAD "  AGGIORNAMENTO FIRMWARE OTA");
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(hdr_lbl, lv_color_black(), 0);
    lv_obj_align(hdr_lbl, LV_ALIGN_CENTER, 0, 0);

    // Label URL
    lv_obj_t* url_lbl = lv_label_create(ui_ScreenOta);
    lv_label_set_text(url_lbl, "URL firmware (.bin):");
    lv_obj_set_style_text_font(url_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(url_lbl, COL_GRAY, 0);
    lv_obj_set_pos(url_lbl, 6, 34);

    // TextArea URL w=472
    ui_OtaUrlTA = lv_textarea_create(ui_ScreenOta);
    lv_obj_set_pos(ui_OtaUrlTA, 4, 50);
    lv_obj_set_size(ui_OtaUrlTA, 472, 38);
    lv_textarea_set_max_length(ui_OtaUrlTA, 255);
    lv_textarea_set_one_line(ui_OtaUrlTA, true);
    lv_textarea_set_placeholder_text(ui_OtaUrlTA, "http://192.168.1.x/firmware.bin");
    lv_obj_set_style_bg_color(ui_OtaUrlTA, COL_DGRAY, 0);
    lv_obj_set_style_bg_opa(ui_OtaUrlTA, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_OtaUrlTA, COL_WHITE, 0);
    lv_obj_set_style_text_font(ui_OtaUrlTA, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(ui_OtaUrlTA, COL_OTA, 0);
    lv_obj_set_style_border_width(ui_OtaUrlTA, 2, 0);
    if (g_ota_url[0]) lv_textarea_set_text(ui_OtaUrlTA, g_ota_url);

    // Tastiera URL — y=90 h=100 w=480 SPECIAL
    ui_OtaKbd = lv_keyboard_create(ui_ScreenOta);
    lv_obj_set_pos(ui_OtaKbd, 0, 90);
    lv_obj_set_size(ui_OtaKbd, 480, 100);
    lv_keyboard_set_mode(ui_OtaKbd, LV_KEYBOARD_MODE_SPECIAL);
    lv_keyboard_set_textarea(ui_OtaKbd, ui_OtaUrlTA);
    lv_obj_set_style_bg_color(ui_OtaKbd, lv_color_make(0x1A, 0x1A, 0x2A), 0);
    lv_obj_set_style_bg_opa(ui_OtaKbd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_OtaKbd, COL_WHITE, 0);
    lv_obj_add_event_cb(ui_OtaKbd, cb_ota_kbd, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_OtaKbd, cb_ota_kbd, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_OtaKbd, cb_ota_kbd, LV_EVENT_CANCEL, NULL);

    // Barra progresso y=194 h=20 w=400
    ui_OtaBar = lv_bar_create(ui_ScreenOta);
    lv_obj_set_pos(ui_OtaBar, 4, 194);
    lv_obj_set_size(ui_OtaBar, 400, 20);
    lv_bar_set_range(ui_OtaBar, 0, 100);
    lv_bar_set_value(ui_OtaBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_OtaBar, COL_DGRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_OtaBar, COL_OTA, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui_OtaBar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_OtaBar, 5, LV_PART_INDICATOR);

    ui_OtaBarLbl = lv_label_create(ui_ScreenOta);
    lv_label_set_text(ui_OtaBarLbl, "0%");
    lv_obj_set_style_text_font(ui_OtaBarLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_OtaBarLbl, COL_WHITE, 0);
    lv_obj_set_pos(ui_OtaBarLbl, 410, 196);

    // Sep y=216
    lv_obj_t* sep = lv_obj_create(ui_ScreenOta);
    lv_obj_set_pos(sep, 0, 216);
    lv_obj_set_size(sep, 480, 2);
    lv_obj_set_style_bg_color(sep, COL_OTA, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    // Pulsanti y=218 h=26
    ui_BtnOtaStart = make_action_btn(ui_ScreenOta,
        2, 218, 236, 26, LV_SYMBOL_UPLOAD " AVVIA OTA", COL_OTA, cb_ota_start_btn);
    ui_BtnOtaBack  = make_action_btn(ui_ScreenOta,
        242, 218, 236, 26, LV_SYMBOL_LEFT " INDIETRO", COL_GRAY, cb_ota_back_btn);

    // Status
    ui_OtaStatusLbl = make_status_lbl(ui_ScreenOta, 248, g_ota_status_msg);
}

// ================================================================
//  NAVIGAZIONE
// ================================================================
void ui_show_wifi_scan(void) {
    ui_wifi_update_status();
    lv_scr_load_anim(ui_ScreenWifi, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void ui_show_wifi_pwd(const char* ssid) {
    strncpy(g_wifi_selected_ssid, ssid, 32);
    g_wifi_selected_ssid[32] = '\0';

    char buf[64];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  %s", ssid);
    lv_label_set_text(ui_WifiPwdSsidLbl, buf);

    lv_textarea_set_text(ui_WifiPwdTA, "");
    lv_label_set_text(ui_WifiPwdStatusLbl, "");

    lv_scr_load_anim(ui_ScreenWifiPwd, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void ui_show_ota(void) {
    ui_ota_update_progress();
    lv_scr_load_anim(ui_ScreenOta, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

// ================================================================
//  AGGIORNAMENTO LISTA RETI
// ================================================================
void ui_wifi_update_list(void) {
    if (!ui_WifiList) return;
    lv_obj_clean(ui_WifiList);

    int n = g_wifi_net_count;
    if (n < 0) {
        lv_list_add_text(ui_WifiList, LV_SYMBOL_REFRESH " Scansione in corso...");
        return;
    }
    if (n == 0) {
        lv_list_add_text(ui_WifiList, "Nessuna rete trovata");
        return;
    }

    for (int i = 0; i < n && i < WIFI_SCAN_MAX; i++) {
        char row[48];
        snprintf(row, sizeof(row), "%s %s%s",
            rssi_symbol(g_wifi_nets[i].rssi),
            g_wifi_nets[i].ssid,
            g_wifi_nets[i].secure ? " " LV_SYMBOL_CLOSE : "");

        lv_obj_t* btn = lv_list_add_btn(ui_WifiList, NULL, row);
        lv_obj_set_style_bg_color(btn, lv_color_make(0x1C, 0x1C, 0x2C), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(btn, COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_make(0x30, 0x30, 0x50), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_14, 0);
        lv_obj_set_height(btn, 24);
        lv_obj_add_event_cb(btn, cb_wifi_net_selected, LV_EVENT_CLICKED,
                            (void*)(uintptr_t)i);
    }
}

// ================================================================
//  AGGIORNAMENTO STATUS CONNESSIONE
// ================================================================
void ui_wifi_update_status(void) {
    if (!ui_WifiStatusLbl) return;
    if (g_wifi_connected) {
        char buf[80];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " Connesso: %s  |  MQTT: %s",
                 WiFi.SSID().c_str(),
                 g_mqtt_connected ? "OK" : "---");
        lv_label_set_text(ui_WifiStatusLbl, buf);
        lv_obj_set_style_text_color(ui_WifiStatusLbl, COL_OK, 0);
    } else {
        lv_label_set_text(ui_WifiStatusLbl, LV_SYMBOL_CLOSE " Non connesso");
        lv_obj_set_style_text_color(ui_WifiStatusLbl, COL_ERR, 0);
    }
}

// ================================================================
//  AGGIORNAMENTO BARRA OTA
// ================================================================
void ui_ota_update_progress(void) {
    if (!ui_OtaBar) return;
    int p = g_ota_progress;
    if (p < 0) {
        lv_bar_set_value(ui_OtaBar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(ui_OtaBar, COL_ERR, LV_PART_INDICATOR);
        lv_label_set_text(ui_OtaBarLbl, "ERR");
        lv_label_set_text(ui_OtaStatusLbl, g_ota_status_msg);
        lv_obj_set_style_text_color(ui_OtaStatusLbl, COL_ERR, 0);
    } else {
        lv_bar_set_value(ui_OtaBar, p, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(ui_OtaBar,
            p == 100 ? COL_OK : COL_OTA, LV_PART_INDICATOR);
        char pct[8];
        snprintf(pct, sizeof(pct), "%d%%", p);
        lv_label_set_text(ui_OtaBarLbl, pct);
        lv_label_set_text(ui_OtaStatusLbl, g_ota_status_msg);
        lv_obj_set_style_text_color(ui_OtaStatusLbl,
            p == 100 ? COL_OK : COL_GRAY, 0);
    }
    if (ui_BtnOtaStart) {
        if (g_ota_running) lv_obj_add_state(ui_BtnOtaStart, LV_STATE_DISABLED);
        else               lv_obj_clear_state(ui_BtnOtaStart, LV_STATE_DISABLED);
    }
}

// ================================================================
//  CALLBACKS — WIFI_SCAN
// ================================================================
static void cb_wifi_scan_btn(lv_event_t* e) {
    g_wifi_net_count    = -1;
    g_wifi_scan_request = true;
    ui_wifi_update_list();
}

static void cb_wifi_back_btn(lv_event_t* e) {
    extern void ui_show_screen(Screen s);
    ui_show_screen(Screen::MAIN);
}

static void cb_wifi_net_selected(lv_event_t* e) {
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= g_wifi_net_count) return;
    const char* ssid = g_wifi_nets[idx].ssid;
    if (g_wifi_nets[idx].secure) {
        ui_show_wifi_pwd(ssid);
    } else {
        strncpy(g_wifi_selected_ssid, ssid, 32);
        g_wifi_selected_ssid[32] = '\0';
        extern void wifi_request_connect(const char* ssid, const char* pass);
        wifi_request_connect(ssid, "");
        lv_label_set_text(ui_WifiStatusLbl, "Connessione in corso...");
        lv_obj_set_style_text_color(ui_WifiStatusLbl, COL_YELLOW, 0);
    }
}

// ================================================================
//  CALLBACKS — WIFI_PWD
// ================================================================
static void cb_wifi_pwd_kbd(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY)  cb_wifi_connect_btn(NULL);
    else if (code == LV_EVENT_CANCEL) ui_show_wifi_scan();
}

static void cb_wifi_connect_btn(lv_event_t* e) {
    const char* pass = lv_textarea_get_text(ui_WifiPwdTA);
    if (!pass || strlen(pass) == 0) {
        lv_label_set_text(ui_WifiPwdStatusLbl, "Password non può essere vuota");
        lv_obj_set_style_text_color(ui_WifiPwdStatusLbl, COL_ERR, 0);
        return;
    }
    extern void wifi_request_connect(const char* ssid, const char* pass);
    wifi_request_connect(g_wifi_selected_ssid, pass);
    lv_label_set_text(ui_WifiPwdStatusLbl, "Connessione in corso...");
    lv_obj_set_style_text_color(ui_WifiPwdStatusLbl, COL_YELLOW, 0);
    lv_timer_t* t = lv_timer_create([](lv_timer_t* t) {
        ui_show_wifi_scan();
        lv_timer_del(t);
    }, 1200, NULL);
    (void)t;
}

static void cb_wifi_cancel_btn(lv_event_t* e) {
    ui_show_wifi_scan();
}

// ================================================================
//  CALLBACKS — OTA
// ================================================================
static void cb_ota_kbd(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_clear_state(ui_OtaUrlTA, LV_STATE_FOCUSED);
    }
}

static void cb_ota_start_btn(lv_event_t* e) {
    if (g_ota_running) return;
    const char* url = lv_textarea_get_text(ui_OtaUrlTA);
    if (!url || strlen(url) < 8 ||
        (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)) {
        lv_label_set_text(ui_OtaStatusLbl, "URL non valido (inizia con http://)");
        lv_obj_set_style_text_color(ui_OtaStatusLbl, COL_ERR, 0);
        return;
    }
    strncpy(g_ota_url, url, 255);
    g_ota_url[255] = '\0';
    g_ota_progress      = 0;
    g_ota_start_request = true;
    strncpy(g_ota_status_msg, "Avvio OTA...", sizeof(g_ota_status_msg));
    ui_ota_update_progress();
}

static void cb_ota_back_btn(lv_event_t* e) {
    if (g_ota_running) {
        lv_label_set_text(ui_OtaStatusLbl, "OTA in corso, attendere...");
        lv_obj_set_style_text_color(ui_OtaStatusLbl, COL_YELLOW, 0);
        return;
    }
    extern void ui_show_screen(Screen s);
    ui_show_screen(Screen::MAIN);
}
