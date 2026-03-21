/**
 * ui_wifi.cpp — Forno Pizza v23-S3
 * FIX LAYOUT: tastiera LVGL posizionata con LV_ALIGN_BOTTOM_MID
 * La tastiera occupa la sua altezza naturale (~160px) ancorata in basso.
 * Tutto il resto (header, label, textarea) sta sopra in 32+50=82px.
 * Nessun bottone separato per CONNETTI/ANNULLA — usa OK/Cancel della tastiera.
 */

#include "ui_wifi.h"
#include "ui.h"
#include "ui_animations.h"
#include <lvgl.h>
#include <WiFi.h>
#include <Preferences.h>


extern volatile bool g_wifi_connected;
extern volatile bool g_mqtt_connected;
extern void wifi_request_connect(const char* ssid, const char* pass);

// ================================================================
//  GLOBALS — widget
// ================================================================
lv_obj_t* ui_ScreenWifi     = NULL;
lv_obj_t* ui_WifiList       = NULL;
static lv_obj_t* s_wifi_btns[WIFI_SCAN_MAX] = {};
static lv_obj_t* s_wifi_status_lbl_list = NULL;
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

lv_obj_t* ui_ScreenOta    = NULL;
lv_obj_t* ui_OtaUrlTA     = NULL;
lv_obj_t* ui_OtaKbd       = NULL;
lv_obj_t* ui_OtaBar       = NULL;
lv_obj_t* ui_OtaBarLbl    = NULL;
lv_obj_t* ui_BtnOtaStart  = NULL;
lv_obj_t* ui_BtnOtaBack   = NULL;
lv_obj_t* ui_OtaStatusLbl = NULL;

lv_obj_t* ui_ScreenMqtt   = NULL;
lv_obj_t* ui_MqttHostTA   = NULL;
lv_obj_t* ui_MqttPortTA   = NULL;
lv_obj_t* ui_MqttUserTA   = NULL;
lv_obj_t* ui_MqttPassTA   = NULL;
lv_obj_t* ui_MqttKbd      = NULL;   // tastierino inserimento
lv_obj_t* ui_BtnMqttToWifi = NULL;
lv_obj_t* ui_BtnMqttToMain = NULL;
lv_obj_t* ui_MqttBtnLbl   = NULL;   // label del pulsante MQTT in header WiFi

#define MQTT_NVS_NAMESPACE "mqtt"
#define MQTT_DEFAULT_PORT  "1883"

// ================================================================
//  GLOBALS — stato scan / OTA
// ================================================================
WifiNetInfo  g_wifi_nets[WIFI_SCAN_MAX] = {};
volatile int g_wifi_net_count           = 0;
char         g_wifi_selected_ssid[33]   = "";
volatile bool g_wifi_scan_request       = false;
volatile bool g_wifi_scan_done          = false;
volatile bool g_wifi_status_changed     = false;

volatile int  g_ota_progress            = 0;
volatile bool g_ota_running             = false;
volatile bool g_ota_start_request       = false;
char          g_ota_url[256]            = "";
char          g_ota_status_msg[64]      = "Inserisci URL e premi AVVIA";

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
//  FORWARD DECLARATIONS
// ================================================================
static void cb_wifi_scan_btn(lv_event_t* e);
static void cb_wifi_back_btn(lv_event_t* e);
static void cb_wifi_net_selected(lv_event_t* e);
void        cb_wifi_pwd_kbd(lv_event_t* e);
static void cb_ota_kbd(lv_event_t* e);
static void cb_ota_start_btn(lv_event_t* e);
static void cb_ota_back_btn(lv_event_t* e);
static void cb_goto_mqtt(lv_event_t* e);
static void cb_mqtt_to_wifi(lv_event_t* e);
static void cb_mqtt_to_main(lv_event_t* e);
static void cb_mqtt_ta_focused(lv_event_t* e);

// ================================================================
//  HELPERS
// ================================================================
static lv_obj_t* make_action_btn(lv_obj_t* parent,
                                  int x, int y, int w, int h,
                                  const char* label,
                                  lv_color_t border_col,
                                  lv_event_cb_t cb) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_make(0x1C,0x1C,0x2C), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, border_col, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    //lv_obj_set_style_transition_duration(b, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, border_col, 0);
    lv_obj_center(l);
    return b;
}

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

static const char* rssi_symbol(int32_t rssi) {
    if (rssi >= -60) return LV_SYMBOL_WIFI;
    if (rssi >= -75) return LV_SYMBOL_WIFI;
    return LV_SYMBOL_CLOSE;
}

static int  s_list_last_n          = -99;
static volatile bool s_net_selection_busy = false;

// ================================================================
//  BUILD — WIFI_SCAN
//  Header y=0..31 h=32 | Lista y=32..197 h=166
//  Sep y=198 | Btns y=201..225 h=25 | Status y=228
// ================================================================
void ui_build_wifi(void) {
    ui_ScreenWifi = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenWifi, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_ScreenWifi, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_ScreenWifi, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hdr = lv_obj_create(ui_ScreenWifi);
    lv_obj_set_pos(hdr, 0, 0); lv_obj_set_size(hdr, 480, 32);
    lv_obj_set_style_bg_color(hdr, COL_WIFI, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* hdr_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_lbl, LV_SYMBOL_WIFI "  RETE WiFi");
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(hdr_lbl, lv_color_white(), 0);
    lv_obj_align(hdr_lbl, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* btn_mqtt = lv_btn_create(hdr);
    lv_obj_set_pos(btn_mqtt, 380, 2);
    lv_obj_set_size(btn_mqtt, 90, 28);
    lv_obj_set_style_bg_color(btn_mqtt, lv_color_make(0x00, 0x50, 0x28), 0);
    lv_obj_set_style_border_color(btn_mqtt, COL_OK, 0);
    lv_obj_set_style_border_width(btn_mqtt, 2, 0);
    lv_obj_set_style_radius(btn_mqtt, 6, 0);
    lv_obj_set_style_shadow_width(btn_mqtt, 0, 0);
    //lv_obj_set_style_transition_duration(btn_mqtt, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_mqtt, cb_goto_mqtt, LV_EVENT_CLICKED, NULL);
    ui_MqttBtnLbl = lv_label_create(btn_mqtt);
    lv_label_set_text(ui_MqttBtnLbl, "MQTT");
    lv_obj_set_style_text_font(ui_MqttBtnLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_MqttBtnLbl, lv_color_white(), 0);
    lv_obj_center(ui_MqttBtnLbl);

    ui_WifiList = lv_obj_create(ui_ScreenWifi);
    lv_obj_set_pos(ui_WifiList, 0, 32); lv_obj_set_size(ui_WifiList, 480, 166);
    lv_obj_set_style_bg_color(ui_WifiList, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_WifiList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_WifiList, 0, 0);
    lv_obj_set_style_pad_all(ui_WifiList, 0, 0);
    lv_obj_set_style_radius(ui_WifiList, 0, 0);
    lv_obj_add_flag(ui_WifiList, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui_WifiList, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(ui_WifiList, LV_DIR_VER);

    s_wifi_status_lbl_list = lv_label_create(ui_WifiList);
    lv_label_set_text(s_wifi_status_lbl_list, "Premi AGGIORNA per cercare reti");
    lv_obj_set_style_text_font(s_wifi_status_lbl_list, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_wifi_status_lbl_list, COL_GRAY, 0);
    lv_obj_set_pos(s_wifi_status_lbl_list, 8, 4);

    for (int i = 0; i < WIFI_SCAN_MAX; i++) {
        lv_obj_t* b = lv_btn_create(ui_WifiList);
        lv_obj_set_pos(b, 2, 2 + i * 26);
        lv_obj_set_size(b, 476, 24);
        lv_obj_set_style_bg_color(b, lv_color_make(0x1C,0x1C,0x2C), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(b, lv_color_make(0x30,0x30,0x50), 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_pad_all(b, 2, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* lbl = lv_label_create(b);
        lv_label_set_text(lbl, "");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xFF,0xFF,0xFF), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 2, 0);
        lv_obj_set_user_data(b, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(b, cb_wifi_net_selected, LV_EVENT_RELEASED, NULL);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
        s_wifi_btns[i] = b;
    }

    lv_obj_t* sep = lv_obj_create(ui_ScreenWifi);
    lv_obj_set_pos(sep, 0, 198); lv_obj_set_size(sep, 480, 2);
    lv_obj_set_style_bg_color(sep, COL_WIFI, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    ui_BtnWifiScan = make_action_btn(ui_ScreenWifi,
        2, 201, 236, 25, LV_SYMBOL_REFRESH " AGGIORNA", COL_WIFI, cb_wifi_scan_btn);
    ui_BtnWifiBack = make_action_btn(ui_ScreenWifi,
        242, 201, 236, 25, LV_SYMBOL_LEFT " INDIETRO", COL_GRAY, cb_wifi_back_btn);

    ui_WifiStatusLbl = make_status_lbl(ui_ScreenWifi, 228, LV_SYMBOL_CLOSE " Non connesso");
    lv_obj_set_style_text_color(ui_WifiStatusLbl, COL_ERR, 0);
}

// ================================================================
//  BUILD — WIFI_PWD
//
//  La tastiera LVGL TEXT_LOWER ha 5 righe di tasti.
//  Con lv_obj_align(..., LV_ALIGN_BOTTOM_MID, 0, 0) occupa
//  la sua altezza naturale (~160px) partendo dal fondo dello schermo.
//  Sopra: header 32px + label 14px + textarea 36px = 82px totali.
//  La tastiera occupa y=272-160=112 in giù — tutto visibile.
//  I bottoni CONNETTI/ANNULLA usano LV_ALIGN_BOTTOM_MID con offset
//  calcolato sopra la tastiera.
//
//  NOTA: non impostiamo h sulla tastiera — LVGL la dimensiona da sola.
// ================================================================
void ui_build_wifi_pwd(void) {
    ui_ScreenWifiPwd = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenWifiPwd, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_ScreenWifiPwd, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_ScreenWifiPwd, LV_OBJ_FLAG_SCROLLABLE);

    // Header y=0..31
    lv_obj_t* hdr = lv_obj_create(ui_ScreenWifiPwd);
    lv_obj_set_pos(hdr, 0, 0); lv_obj_set_size(hdr, 480, 32);
    lv_obj_set_style_bg_color(hdr, COL_WIFI, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    ui_WifiPwdSsidLbl = lv_label_create(hdr);
    lv_label_set_text(ui_WifiPwdSsidLbl, LV_SYMBOL_WIFI "  ---");
    lv_obj_set_style_text_font(ui_WifiPwdSsidLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_WifiPwdSsidLbl, lv_color_white(), 0);
    lv_obj_align(ui_WifiPwdSsidLbl, LV_ALIGN_CENTER, 0, 0);

    // Label y=34
    lv_obj_t* pwd_lbl = lv_label_create(ui_ScreenWifiPwd);
    lv_label_set_text(pwd_lbl, "Password:");
    lv_obj_set_style_text_font(pwd_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pwd_lbl, COL_GRAY, 0);
    lv_obj_set_pos(pwd_lbl, 6, 34);

    // TextArea y=50 h=36
    ui_WifiPwdTA = lv_textarea_create(ui_ScreenWifiPwd);
    lv_obj_set_pos(ui_WifiPwdTA, 4, 50); lv_obj_set_size(ui_WifiPwdTA, 472, 36);
    lv_textarea_set_password_mode(ui_WifiPwdTA, true);
    lv_textarea_set_max_length(ui_WifiPwdTA, 63);
    lv_textarea_set_one_line(ui_WifiPwdTA, true);
    lv_textarea_set_placeholder_text(ui_WifiPwdTA, "Inserisci password...");
    lv_obj_set_style_bg_color(ui_WifiPwdTA, COL_DGRAY, 0);
    lv_obj_set_style_bg_opa(ui_WifiPwdTA, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_WifiPwdTA, COL_WHITE, 0);
    lv_obj_set_style_text_font(ui_WifiPwdTA, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(ui_WifiPwdTA, COL_WIFI, 0);
    lv_obj_set_style_border_width(ui_WifiPwdTA, 2, 0);

    // Tastiera ancorata in basso — LVGL sceglie la sua altezza naturale
    // Non impostiamo h: lv_keyboard si autodimensiona con LV_SIZE_CONTENT
    ui_WifiKbd = lv_keyboard_create(ui_ScreenWifiPwd);
    lv_obj_set_width(ui_WifiKbd, 480);
    lv_obj_align(ui_WifiKbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(ui_WifiKbd, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(ui_WifiKbd, ui_WifiPwdTA);
    lv_obj_set_style_bg_color(ui_WifiKbd, lv_color_make(0x1A,0x1A,0x2A), 0);
    lv_obj_set_style_bg_opa(ui_WifiKbd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_WifiKbd, COL_WHITE, 0);
    lv_obj_add_event_cb(ui_WifiKbd, cb_wifi_pwd_kbd, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_WifiKbd, cb_wifi_pwd_kbd, LV_EVENT_CANCEL, NULL);

    // ui_BtnWifiConnect / Cancel: non servono, la tastiera ha già OK e Cancel
    // ma li creiamo hidden per compatibilità con il resto del codice
    ui_BtnWifiConnect = lv_btn_create(ui_ScreenWifiPwd);
    lv_obj_add_flag(ui_BtnWifiConnect, LV_OBJ_FLAG_HIDDEN);
    ui_BtnWifiCancel  = lv_btn_create(ui_ScreenWifiPwd);
    lv_obj_add_flag(ui_BtnWifiCancel, LV_OBJ_FLAG_HIDDEN);

    ui_WifiPwdStatusLbl = lv_label_create(ui_ScreenWifiPwd);
    lv_label_set_text(ui_WifiPwdStatusLbl, "");
    lv_obj_set_style_text_font(ui_WifiPwdStatusLbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(ui_WifiPwdStatusLbl, COL_GRAY, 0);
    lv_obj_add_flag(ui_WifiPwdStatusLbl, LV_OBJ_FLAG_HIDDEN);
}

// ================================================================
//  BUILD — OTA
//
//  Stessa strategia: tastiera SPECIAL ancorata in basso.
//  La tastiera SPECIAL ha solo 2 righe (simboli URL) — occupa ~80px.
//  Sopra: header 32 + label 14 + textarea 36 = 82px.
//  Tra textarea e tastiera: barra OTA + bottoni nello spazio libero.
//
//  Layout:
//  Header        y=0..31   h=32
//  Lbl URL       y=34      h=14
//  TextArea      y=50..85  h=36
//  Barra OTA     y=90..104 h=16   (visibile quando non si digita)
//  % label       y=90
//  Btn AVVIA     y=108..130 h=24
//  Btn INDIETRO  y=108..130 h=24
//  Status        y=134
//  Tastiera      ancorata BOTTOM_MID, ~80px — y=192..272
// ================================================================
void ui_build_ota(void) {
    ui_ScreenOta = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenOta, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_ScreenOta, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_ScreenOta, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hdr = lv_obj_create(ui_ScreenOta);
    lv_obj_set_pos(hdr, 0, 0); lv_obj_set_size(hdr, 480, 32);
    lv_obj_set_style_bg_color(hdr, COL_OTA, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* hdr_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_lbl, LV_SYMBOL_UPLOAD "  OTA FIRMWARE");
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hdr_lbl, lv_color_black(), 0);
    lv_obj_align(hdr_lbl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* url_lbl = lv_label_create(ui_ScreenOta);
    lv_label_set_text(url_lbl, "URL firmware (.bin):");
    lv_obj_set_style_text_font(url_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(url_lbl, COL_GRAY, 0);
    lv_obj_set_pos(url_lbl, 6, 34);

    ui_OtaUrlTA = lv_textarea_create(ui_ScreenOta);
    lv_obj_set_pos(ui_OtaUrlTA, 4, 50); lv_obj_set_size(ui_OtaUrlTA, 472, 36);
    lv_textarea_set_max_length(ui_OtaUrlTA, 255);
    lv_textarea_set_one_line(ui_OtaUrlTA, true);
    lv_textarea_set_placeholder_text(ui_OtaUrlTA, "http://192.168.1.x/firmware.bin");
    lv_obj_set_style_bg_color(ui_OtaUrlTA, COL_DGRAY, 0);
    lv_obj_set_style_bg_opa(ui_OtaUrlTA, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_OtaUrlTA, COL_WHITE, 0);
    lv_obj_set_style_text_font(ui_OtaUrlTA, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_color(ui_OtaUrlTA, COL_OTA, 0);
    lv_obj_set_style_border_width(ui_OtaUrlTA, 2, 0);
    if (g_ota_url[0]) lv_textarea_set_text(ui_OtaUrlTA, g_ota_url);

    // Barra OTA y=90 h=16
    ui_OtaBar = lv_bar_create(ui_ScreenOta);
    lv_obj_set_pos(ui_OtaBar, 4, 90); lv_obj_set_size(ui_OtaBar, 390, 16);
    lv_bar_set_range(ui_OtaBar, 0, 100);
    lv_bar_set_value(ui_OtaBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_OtaBar, COL_DGRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_OtaBar, COL_OTA,   LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui_OtaBar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_OtaBar, 4, LV_PART_INDICATOR);

    ui_OtaBarLbl = lv_label_create(ui_ScreenOta);
    lv_label_set_text(ui_OtaBarLbl, "0%");
    lv_obj_set_style_text_font(ui_OtaBarLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_OtaBarLbl, COL_OTA, 0);
    lv_obj_set_pos(ui_OtaBarLbl, 398, 90);

    // Bottoni AVVIA / INDIETRO y=110 h=26
    ui_BtnOtaStart = make_action_btn(ui_ScreenOta,
        2, 110, 236, 26, LV_SYMBOL_PLAY " AVVIA OTA", COL_OTA, cb_ota_start_btn);
    ui_BtnOtaBack  = make_action_btn(ui_ScreenOta,
        242, 110, 236, 26, LV_SYMBOL_LEFT " INDIETRO", COL_GRAY, cb_ota_back_btn);

    // Status y=140
    ui_OtaStatusLbl = lv_label_create(ui_ScreenOta);
    lv_label_set_text(ui_OtaStatusLbl, "");
    lv_obj_set_style_text_font(ui_OtaStatusLbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(ui_OtaStatusLbl, COL_GRAY, 0);
    lv_obj_set_width(ui_OtaStatusLbl, 476);
    lv_obj_set_style_text_align(ui_OtaStatusLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(ui_OtaStatusLbl, 2, 140);

    // Tastiera ancorata in basso — SPECIAL ha ~2 righe, ~80px
    ui_OtaKbd = lv_keyboard_create(ui_ScreenOta);
    lv_obj_set_width(ui_OtaKbd, 480);
    lv_obj_align(ui_OtaKbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(ui_OtaKbd, LV_KEYBOARD_MODE_SPECIAL);
    lv_keyboard_set_textarea(ui_OtaKbd, ui_OtaUrlTA);
    lv_obj_set_style_bg_color(ui_OtaKbd, lv_color_make(0x1A,0x1A,0x2A), 0);
    lv_obj_set_style_bg_opa(ui_OtaKbd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_OtaKbd, COL_WHITE, 0);
    lv_obj_add_event_cb(ui_OtaKbd, cb_ota_kbd, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_OtaKbd, cb_ota_kbd, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_OtaKbd, cb_ota_kbd, LV_EVENT_CANCEL, NULL);
}

// ================================================================
//  BUILD — MQTT (broker: IP, porta, user, password)
//  Layout: header + griglia 2 righe x 2 colonne (campi dimezzati) + spazio tastierino + barra nav
// ================================================================
static void mqtt_nvs_load(char* host, size_t host_sz, char* port, size_t port_sz,
                          char* user, size_t user_sz, char* pass, size_t pass_sz) {
    Preferences prefs;
    if (!prefs.begin(MQTT_NVS_NAMESPACE, true)) {
        if (host_sz) { strncpy(host, "192.168.1.100", host_sz - 1); host[host_sz - 1] = '\0'; }
        if (port_sz) { strncpy(port, MQTT_DEFAULT_PORT, port_sz - 1); port[port_sz - 1] = '\0'; }
        if (user_sz) user[0] = '\0';
        if (pass_sz) pass[0] = '\0';
        return;
    }
    if (host_sz) { strncpy(host, prefs.getString("host", "192.168.1.100").c_str(), host_sz - 1); host[host_sz - 1] = '\0'; }
    if (port_sz) { strncpy(port, prefs.getString("port", MQTT_DEFAULT_PORT).c_str(), port_sz - 1); port[port_sz - 1] = '\0'; }
    if (user_sz) { strncpy(user, prefs.getString("user", "").c_str(), user_sz - 1); user[user_sz - 1] = '\0'; }
    if (pass_sz) { strncpy(pass, prefs.getString("pass", "").c_str(), pass_sz - 1); pass[pass_sz - 1] = '\0'; }
    prefs.end();
}

static void mqtt_nvs_save(const char* host, const char* port, const char* user, const char* pass) {
    Preferences prefs;
    if (!prefs.begin(MQTT_NVS_NAMESPACE, false)) return;
    if (host) prefs.putString("host", host);
    if (port) prefs.putString("port", port);
    if (user) prefs.putString("user", user);
    if (pass) prefs.putString("pass", pass);
    prefs.end();
}

static void cb_mqtt_ta_focused(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target(e);
    if (ui_MqttKbd && ta) {
        lv_keyboard_set_textarea(ui_MqttKbd, ta);
        lv_obj_clear_flag(ui_MqttKbd, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_build_mqtt(void) {
    lv_color_t COL_MQTT = lv_color_make(0x00, 0x99, 0x55);
    const int CW = 230;  // larghezza colonna (2 colonne: 4 + 230 + 8 + 230 + 8 = 480)
    const int MARGIN = 4;
    const int GAP = 8;
    const int ROW1_Y = 36;
    const int ROW2_Y = 92;
    const int LABEL_H = 14;
    const int TA_H = 28;

    ui_ScreenMqtt = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenMqtt, COL_DARK, 0);
    lv_obj_set_style_bg_opa(ui_ScreenMqtt, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_ScreenMqtt, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hdr = lv_obj_create(ui_ScreenMqtt);
    lv_obj_set_pos(hdr, 0, 0); lv_obj_set_size(hdr, 480, 32);
    lv_obj_set_style_bg_color(hdr, COL_MQTT, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    ui_BtnMqttToWifi = make_action_btn(hdr, 2, 2, 72, 28,
        LV_SYMBOL_LEFT " WiFi", COL_WIFI, cb_mqtt_to_wifi);
    ui_BtnMqttToMain = make_action_btn(hdr, 406, 2, 72, 28,
        "Main " LV_SYMBOL_RIGHT, COL_GRAY, cb_mqtt_to_main);

    lv_obj_t* hdr_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_lbl, "Broker MQTT");
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(hdr_lbl, lv_color_white(), 0);
    lv_obj_align(hdr_lbl, LV_ALIGN_CENTER, 0, 0);

    auto add_cell = [&](int x, int y, const char* label_txt, lv_obj_t** ta_out, int max_len, bool password) {
        lv_obj_t* lbl = lv_label_create(ui_ScreenMqtt);
        lv_label_set_text(lbl, label_txt);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COL_GRAY, 0);
        lv_obj_set_pos(lbl, x, y);
        lv_obj_t* ta = lv_textarea_create(ui_ScreenMqtt);
        lv_obj_set_pos(ta, x, y + LABEL_H + 2);
        lv_obj_set_size(ta, CW, TA_H);
        lv_textarea_set_max_length(ta, max_len);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_password_mode(ta, password);
        lv_obj_set_style_bg_color(ta, COL_DGRAY, 0);
        lv_obj_set_style_text_color(ta, COL_WHITE, 0);
        lv_obj_set_style_border_color(ta, COL_MQTT, 0);
        lv_obj_set_style_border_width(ta, 2, 0);
        lv_obj_add_event_cb(ta, cb_mqtt_ta_focused, LV_EVENT_FOCUSED, NULL);
        *ta_out = ta;
    };

    int x1 = MARGIN;
    int x2 = MARGIN + CW + GAP;
    add_cell(x1, ROW1_Y, "IP / Host:", &ui_MqttHostTA, 64, false);
    add_cell(x2, ROW1_Y, "Porta:", &ui_MqttPortTA, 6, false);
    if (ui_MqttPortTA) lv_textarea_set_placeholder_text(ui_MqttPortTA, "1883");
    add_cell(x1, ROW2_Y, "Username:", &ui_MqttUserTA, 32, false);
    add_cell(x2, ROW2_Y, "Password:", &ui_MqttPassTA, 32, true);

    // Tastierino in basso — più spazio recuperato (nessuna barra nav sotto)
    ui_MqttKbd = lv_keyboard_create(ui_ScreenMqtt);
    lv_obj_set_width(ui_MqttKbd, 480);
    lv_obj_align(ui_MqttKbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(ui_MqttKbd, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(ui_MqttKbd, ui_MqttHostTA);
    lv_obj_set_style_bg_color(ui_MqttKbd, lv_color_make(0x1A,0x1A,0x2A), 0);
    lv_obj_set_style_bg_opa(ui_MqttKbd, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ui_MqttKbd, COL_WHITE, 0);
    lv_obj_add_flag(ui_MqttKbd, LV_OBJ_FLAG_HIDDEN);
}

// ================================================================
//  NAVIGAZIONE
// ================================================================
void ui_show_wifi_scan(void) {
    s_net_selection_busy = false;
    s_list_last_n = -99;
    ui_wifi_update_status();
    lv_scr_load(ui_ScreenWifi);
}

void ui_show_wifi_pwd(const char* ssid) {
    strncpy(g_wifi_selected_ssid, ssid, 32);
    g_wifi_selected_ssid[32] = '\0';
    char buf[64];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  %s", ssid);
    lv_label_set_text(ui_WifiPwdSsidLbl, buf);
    lv_textarea_set_text(ui_WifiPwdTA, "");
    lv_label_set_text(ui_WifiPwdStatusLbl, "");
    lv_scr_load(ui_ScreenWifiPwd);
}

void ui_show_ota(void) {
    ui_ota_update_progress();
    lv_scr_load(ui_ScreenOta);
}

void ui_show_mqtt(void) {
    char host[65], port[8], user[33], pass[33];
    mqtt_nvs_load(host, sizeof(host), port, sizeof(port), user, sizeof(user), pass, sizeof(pass));
    if (ui_MqttHostTA) lv_textarea_set_text(ui_MqttHostTA, host);
    if (ui_MqttPortTA) lv_textarea_set_text(ui_MqttPortTA, port);
    if (ui_MqttUserTA) lv_textarea_set_text(ui_MqttUserTA, user);
    if (ui_MqttPassTA) lv_textarea_set_text(ui_MqttPassTA, pass);
    if (ui_MqttKbd) lv_obj_add_flag(ui_MqttKbd, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(ui_ScreenMqtt);
}

// ================================================================
//  AGGIORNAMENTO LISTA RETI
// ================================================================
void ui_wifi_update_list(void) {
    if (!ui_WifiList) return;
    int n = g_wifi_net_count;
    if (n == s_list_last_n) return;
    s_list_last_n = n;
    if (s_wifi_status_lbl_list) {
        if (n < 0) {
            lv_label_set_text(s_wifi_status_lbl_list, LV_SYMBOL_REFRESH " Scansione in corso...");
            lv_obj_clear_flag(s_wifi_status_lbl_list, LV_OBJ_FLAG_HIDDEN);
        } else if (n == 0) {
            lv_label_set_text(s_wifi_status_lbl_list, "Nessuna rete trovata");
            lv_obj_clear_flag(s_wifi_status_lbl_list, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_wifi_status_lbl_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
    int show = (n > 0) ? n : 0;
    if (show > WIFI_SCAN_MAX) show = WIFI_SCAN_MAX;
    for (int i = 0; i < WIFI_SCAN_MAX; i++) {
        if (!s_wifi_btns[i]) continue;
        if (i < show) {
            char row[48];
            snprintf(row, sizeof(row), "%s %s%s",
                rssi_symbol(g_wifi_nets[i].rssi),
                g_wifi_nets[i].ssid,
                g_wifi_nets[i].secure ? " " LV_SYMBOL_CLOSE : "");
            lv_obj_t* lbl = lv_obj_get_child(s_wifi_btns[i], 0);
            if (lbl) lv_label_set_text(lbl, row);
            lv_obj_clear_flag(s_wifi_btns[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_wifi_btns[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_wifi_update_status(void) {
    if (!ui_WifiStatusLbl) return;
    if (g_wifi_connected) {
        char buf[96];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " Connesso: %s  |  IP: %s  |  MQTT: %s",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), g_mqtt_connected ? "OK" : "---");
        lv_label_set_text(ui_WifiStatusLbl, buf);
        lv_obj_set_style_text_color(ui_WifiStatusLbl, COL_OK, 0);
    } else {
        lv_label_set_text(ui_WifiStatusLbl, LV_SYMBOL_CLOSE " Non connesso");
        lv_obj_set_style_text_color(ui_WifiStatusLbl, COL_ERR, 0);
    }
}

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
        lv_obj_set_style_bg_color(ui_OtaBar, p == 100 ? COL_OK : COL_OTA, LV_PART_INDICATOR);
        char pct[8]; snprintf(pct, sizeof(pct), "%d%%", p);
        lv_label_set_text(ui_OtaBarLbl, pct);
        lv_bar_set_value(ui_OtaBar, p, LV_ANIM_ON);
        lv_label_set_text(ui_OtaStatusLbl, g_ota_status_msg);
        lv_obj_set_style_text_color(ui_OtaStatusLbl, p == 100 ? COL_OK : COL_GRAY, 0);
    }
    if (ui_BtnOtaStart) {
        if (g_ota_running) lv_obj_add_state(ui_BtnOtaStart, LV_STATE_DISABLED);
        else               lv_obj_clear_state(ui_BtnOtaStart, LV_STATE_DISABLED);
    }
}

// ================================================================
//  CALLBACKS
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
    if (s_net_selection_busy) return;
    lv_obj_t* btn = lv_event_get_target(e);
    int idx = (int)(uintptr_t)lv_obj_get_user_data(btn);
    if (idx < 0 || idx >= g_wifi_net_count) return;
    s_net_selection_busy = true;
    if (g_wifi_nets[idx].secure) {
        ui_show_wifi_pwd(g_wifi_nets[idx].ssid);
    } else {
        strncpy(g_wifi_selected_ssid, g_wifi_nets[idx].ssid, 32);
        g_wifi_selected_ssid[32] = '\0';
        wifi_request_connect(g_wifi_nets[idx].ssid, "");
        extern void ui_show_screen(Screen s);
        ui_show_screen(Screen::MAIN);
    }
}
void cb_wifi_pwd_kbd(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        const char* pwd = lv_textarea_get_text(ui_WifiPwdTA);
        wifi_request_connect(g_wifi_selected_ssid, pwd);
        extern void ui_show_screen(Screen s);
        ui_show_screen(Screen::MAIN);
    } else if (code == LV_EVENT_CANCEL) {
        ui_show_wifi_scan();
    }
}
static void cb_ota_kbd(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_clear_state(ui_OtaUrlTA, LV_STATE_FOCUSED);
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
    strncpy(g_ota_url, url, 255); g_ota_url[255] = '\0';
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

static void cb_goto_mqtt(lv_event_t* e) {
    (void)e;
    extern void ui_show_screen(Screen s);
    ui_show_screen(Screen::MQTT);
}

static void mqtt_save_and_go(Screen target) {
    const char* host = ui_MqttHostTA ? lv_textarea_get_text(ui_MqttHostTA) : "";
    const char* port = ui_MqttPortTA ? lv_textarea_get_text(ui_MqttPortTA) : MQTT_DEFAULT_PORT;
    const char* user = ui_MqttUserTA ? lv_textarea_get_text(ui_MqttUserTA) : "";
    const char* pass = ui_MqttPassTA ? lv_textarea_get_text(ui_MqttPassTA) : "";
    if (!port || !port[0]) port = MQTT_DEFAULT_PORT;
    mqtt_nvs_save(host, port, user, pass);
    extern void ui_show_screen(Screen s);
    ui_show_screen(target);
}

static void cb_mqtt_to_wifi(lv_event_t* e) {
    (void)e;
    mqtt_save_and_go(Screen::WIFI_SCAN);
}

static void cb_mqtt_to_main(lv_event_t* e) {
    (void)e;
    mqtt_save_and_go(Screen::MAIN);
}
