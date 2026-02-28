/**
 * ui.cpp — Forno Pizza Controller v21-S3
 * ================================================================
 * Layout 480×272 — Display RGB565 parallelo (SC7283 / TST043WQHS-67)
 *
 * MAIN (solo lettura):
 *   y=0..31    NavBar h=32 — CTRL | GRAF | WiFi | OTA
 *   y=33..213  pannelli BASE + CIELO — font64 temperatura corrente
 *              w=234 ciascuno con gap 8px centrale (x=2 e x=244)
 *              LED relay, barra PID, preheat indicator
 *   y=215..216 sep h=2
 *   y=217..247 ON/OFF bar h=31 — BASE | CIELO | LUCE
 *   y=249..271 status bar h=23
 *
 * CTRL (setpoint + mode/split):
 *   y=0..31    Header h=32 verde "⚙ CONTROLLO SETPOINT" + ← OK
 *   y=33..183  pannelli BASE + CIELO — arco 120px, font32 setpoint,
 *              bottoni ±5 100×52px, barra PID  h=151
 *   y=184..185 sep
 *   y=186..237 ON/OFF bar h=52 — BASE(116) | CIELO(116) | LUCE(76) | ← MAIN(76)
 *              + nav azioni: RICETTE | TIMER | PID
 *   y=239..271 mode + split h=33
 *
 * PID_BASE / PID_CIELO:
 *   y=0..31    header + nav
 *   Kp y=33 h=74  Ki y=107 h=74  Kd y=181 h=74
 *   Footer y=255..271
 *
 * GRAPH: chart più grande 448×180, 3 preset in basso
 * TIMER / RICETTE: adattati alla larghezza
 * ================================================================
 */

#include "ui.h"
#include "ui_wifi.h"

// ================================================================
//  DIMENSIONI SCHERMO
// ================================================================
#define SCR_W  480
#define SCR_H  272

// ================================================================
//  GLOBALS
// ================================================================
lv_obj_t* ui_ScreenMain     = NULL;
lv_obj_t* ui_ScreenTimer    = NULL;
lv_obj_t* ui_LabelTimerValue  = NULL;
lv_obj_t* ui_LabelTimerStatus = NULL;
lv_obj_t* ui_ScreenRicette  = NULL;
lv_obj_t* ui_LabelTempBase  = NULL;  lv_obj_t* ui_LabelTempCielo = NULL;
lv_obj_t* ui_LabelPidBase   = NULL;  lv_obj_t* ui_LabelPidCielo  = NULL;
lv_obj_t* ui_BarPidBase     = NULL;  lv_obj_t* ui_BarPidCielo    = NULL;
lv_obj_t* ui_LedRelayBase   = NULL;  lv_obj_t* ui_LedRelayCielo  = NULL;
lv_obj_t* ui_LabelErrBase   = NULL;  lv_obj_t* ui_LabelErrCielo  = NULL;
lv_obj_t* ui_BtnEnBase      = NULL;  lv_obj_t* ui_LabelBtnBase   = NULL;
lv_obj_t* ui_BtnEnCielo     = NULL;  lv_obj_t* ui_LabelBtnCielo  = NULL;
lv_obj_t* ui_BtnLuce        = NULL;  lv_obj_t* ui_LabelBtnLuce   = NULL;
lv_obj_t* ui_LedFan         = NULL;  lv_obj_t* ui_LabelStatus    = NULL;
lv_obj_t* ui_PanelSafety    = NULL;  lv_obj_t* ui_LabelSafety    = NULL;

lv_obj_t* ui_ScreenTemp      = NULL;
lv_obj_t* ui_ArcBase         = NULL;  lv_obj_t* ui_ArcCielo        = NULL;
lv_obj_t* ui_TempSetBase     = NULL;  lv_obj_t* ui_TempSetCielo    = NULL;
lv_obj_t* ui_TempBarBase     = NULL;  lv_obj_t* ui_TempBarCielo    = NULL;
lv_obj_t* ui_TempErrBase     = NULL;  lv_obj_t* ui_TempErrCielo    = NULL;
lv_obj_t* ui_TempBtnEnBase   = NULL;  lv_obj_t* ui_TempLblBtnBase  = NULL;
lv_obj_t* ui_TempBtnEnCielo  = NULL;  lv_obj_t* ui_TempLblBtnCielo = NULL;
lv_obj_t* ui_TempBtnLuce     = NULL;  lv_obj_t* ui_TempLblBtnLuce  = NULL;
lv_obj_t* ui_BtnModeToggle   = NULL;  lv_obj_t* ui_LabelMode       = NULL;
lv_obj_t* ui_PanelSplit      = NULL;
lv_obj_t* ui_LabelPctBase    = NULL;
lv_obj_t* ui_LabelPctCielo   = NULL;
lv_obj_t* ui_BtnPctBaseMinus = NULL;  lv_obj_t* ui_BtnPctBasePlus  = NULL;
lv_obj_t* ui_BtnPctCieloMinus= NULL;  lv_obj_t* ui_BtnPctCieloPlus = NULL;
lv_obj_t* ui_TempModeLbl     = NULL;  lv_obj_t* ui_TempStatusLbl   = NULL;
lv_obj_t* ui_TempFanLbl      = NULL;  lv_obj_t* ui_TempSafetyLbl   = NULL;
lv_obj_t* ui_LabelSplitCielo = NULL;  // unused ma mantenuto per compatibilità
lv_obj_t* ui_BtnSplitMinus   = NULL;
lv_obj_t* ui_BtnSplitPlus    = NULL;

lv_obj_t* ui_ScreenPidBase      = NULL;
lv_obj_t* ui_LblKpBase          = NULL;
lv_obj_t* ui_LblKiBase          = NULL;
lv_obj_t* ui_LblKdBase          = NULL;
lv_obj_t* ui_LblSetBaseTun      = NULL;
lv_obj_t* ui_LblSaveStatus      = NULL;

lv_obj_t* ui_ScreenPidCielo     = NULL;
lv_obj_t* ui_LblKpCielo         = NULL;
lv_obj_t* ui_LblKiCielo         = NULL;
lv_obj_t* ui_LblKdCielo         = NULL;
lv_obj_t* ui_LblSetCieloTun     = NULL;
lv_obj_t* ui_LblSaveStatusCielo = NULL;

lv_obj_t* ui_ScreenGraph  = NULL;
lv_obj_t* ui_Chart        = NULL;
lv_chart_series_t* ui_SerBase  = NULL;
lv_chart_series_t* ui_SerCielo = NULL;
lv_obj_t* ui_GraphTimeLbl = NULL;
lv_obj_t* ui_GraphMaxLbl  = NULL;
lv_obj_t* ui_GraphMinLbl  = NULL;
lv_obj_t* ui_BtnPreset5   = NULL;
lv_obj_t* ui_BtnPreset15  = NULL;
lv_obj_t* ui_BtnPreset30  = NULL;
int       g_graph_minutes  = 15;

GraphBuffer g_graph = {{0},{0},0,0};

// ================================================================
//  CALLBACKS — dichiarate in ui_events.cpp
// ================================================================
extern void cb_toggle_base(lv_event_t*);
extern void cb_toggle_cielo(lv_event_t*);
extern void cb_toggle_luce(lv_event_t*);
extern void cb_base_minus(lv_event_t*);
extern void cb_base_plus(lv_event_t*);
extern void cb_cielo_minus(lv_event_t*);
extern void cb_cielo_plus(lv_event_t*);
extern void cb_toggle_mode(lv_event_t*);
extern void cb_mode_long_press(lv_event_t*);
extern void cb_pct_base_minus(lv_event_t*);
extern void cb_pct_base_plus(lv_event_t*);
extern void cb_pct_cielo_minus(lv_event_t*);
extern void cb_pct_cielo_plus(lv_event_t*);
extern void cb_pid_save(lv_event_t*);
extern void cb_goto_temp(lv_event_t*);
extern void cb_goto_main_from_temp(lv_event_t*);
extern void cb_goto_pid_base(lv_event_t*);
extern void cb_goto_pid_cielo(lv_event_t*);
extern void cb_goto_graph(lv_event_t*);
extern void cb_goto_main_from_graph(lv_event_t*);
extern void cb_preset_5(lv_event_t*);
extern void cb_preset_15(lv_event_t*);
extern void cb_preset_30(lv_event_t*);
extern void cb_kp_base_m(lv_event_t*);  extern void cb_kp_base_p(lv_event_t*);
extern void cb_ki_base_m(lv_event_t*);  extern void cb_ki_base_p(lv_event_t*);
extern void cb_kd_base_m(lv_event_t*);  extern void cb_kd_base_p(lv_event_t*);
extern void cb_kp_cielo_m(lv_event_t*); extern void cb_kp_cielo_p(lv_event_t*);
extern void cb_ki_cielo_m(lv_event_t*); extern void cb_ki_cielo_p(lv_event_t*);
extern void cb_kd_cielo_m(lv_event_t*); extern void cb_kd_cielo_p(lv_event_t*);
extern void cb_goto_wifi(lv_event_t*);
extern void cb_goto_ota(lv_event_t*);
extern void cb_goto_timer(lv_event_t*);
extern void cb_goto_ricette(lv_event_t*);
extern void cb_timer_plus(lv_event_t*);
extern void cb_timer_minus(lv_event_t*);
extern void cb_timer_minus15(lv_event_t*);
extern void cb_timer_plus15(lv_event_t*);
extern void cb_timer_reset(lv_event_t*);
extern void cb_ricetta_select(lv_event_t*);

// ================================================================
//  HELPERS
// ================================================================
static lv_obj_t* make_screen(lv_color_t bg) {
    lv_obj_t* s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, bg, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

// Header a tutta larghezza 480px, h=32
static void make_header(lv_obj_t* scr, lv_color_t col, const char* txt) {
    lv_obj_t* h = lv_obj_create(scr);
    lv_obj_set_pos(h, 0, 0); lv_obj_set_size(h, SCR_W, 32);
    lv_obj_set_style_bg_color(h, col, 0);
    lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(h, 0, 0);
    lv_obj_set_style_radius(h, 0, 0);
    lv_obj_set_style_pad_all(h, 0, 0);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* l = lv_label_create(h);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_black(), 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
}

static void make_sep(lv_obj_t* scr, int y, lv_color_t col) {
    lv_obj_t* s = lv_obj_create(scr);
    lv_obj_set_pos(s, 0, y); lv_obj_set_size(s, SCR_W, 2);
    lv_obj_set_style_bg_color(s, col, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_radius(s, 0, 0);
}

static lv_obj_t* make_arc(lv_obj_t* p, int x, int y, int sz, lv_color_t col) {
    lv_obj_t* a = lv_arc_create(p);
    lv_obj_set_pos(a, x, y); lv_obj_set_size(a, sz, sz);
    lv_arc_set_range(a, 0, 500); lv_arc_set_value(a, 0);
    lv_arc_set_angles(a, 135, 405); lv_arc_set_bg_angles(a, 135, 405);
    lv_obj_set_style_arc_color(a, lv_color_make(0x28,0x28,0x40), LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, col, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a, 10, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    return a;
}

static lv_obj_t* make_bar(lv_obj_t* p, int x, int y, int w, int h, lv_color_t col) {
    lv_obj_t* b = lv_bar_create(p);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_bar_set_range(b, 0, 100); lv_bar_set_value(b, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(b, lv_color_make(0x28,0x28,0x3C), LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, col, LV_PART_INDICATOR);
    lv_obj_set_style_radius(b, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 5, LV_PART_INDICATOR);
    return b;
}

// Pulsante +/- con font grande
static lv_obj_t* make_pm(lv_obj_t* p, int x, int y, int w, int h,
                          const char* t, lv_color_t col, lv_event_cb_t cb) {
    lv_obj_t* b = lv_btn_create(p);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_make(0x1C,0x1C,0x2C), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, col, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_center(l);
    return b;
}

// Bottone controllo (ON/OFF, nav) con label esterna opzionale
static lv_obj_t* make_ctrl(lv_obj_t* p, int x, int y, int w, int h,
                            const char* t, lv_event_cb_t cb, lv_obj_t** lout) {
    lv_obj_t* b = lv_btn_create(p);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_make(0x20,0x20,0x2E), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, lv_color_make(0x48,0x48,0x60), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_color_t def_col = lv_color_make(0xA0,0xA0,0xB8);
    lv_obj_set_style_text_color(b, def_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(b, def_col, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(b, def_col, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(b, def_col, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, def_col, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(l, def_col, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(l, def_col, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, w - 8);
    lv_obj_center(l);
    if (lout) *lout = l;
    return b;
}

// ================================================================
//  BUILD MAIN — solo lettura: temperature grandi font64
// ================================================================
// Layout 480×272:
//   NavBar      y=0..31    h=32  — 4 bottoni 120px ciascuno
//   Panel BASE  y=33..212  h=180 x=2   w=234
//   Panel CIELO y=33..212  h=180 x=244 w=234
//   Sep         y=213..214 h=2
//   ON/OFF bar  y=215..247 h=33  BASE(156)|CIELO(156)|LUCE(162)
//   Status bar  y=249..271 h=23
// ================================================================
static void build_main() {
    ui_ScreenMain = make_screen(UI_COL_BG);

    // ── NavBar y=0 h=32 ──
    lv_obj_t* nav = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(nav, 0, 0); lv_obj_set_size(nav, SCR_W, 32);
    lv_obj_set_style_bg_color(nav, lv_color_make(0x08,0x08,0x16), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_radius(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 0, 0);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    // NavBar: CTRL(120) | GRAF(120) | WiFi(120) | OTA(120) = 480px
    auto make_nav_btn = [&](int x, int w,
                             lv_color_t bg, lv_color_t border,
                             const char* text, lv_color_t tcol,
                             lv_event_cb_t cb) {
        lv_obj_t* b = lv_btn_create(nav);
        lv_obj_set_pos(b, x, 0); lv_obj_set_size(b, w, 32);
        lv_obj_set_style_bg_color(b, bg, 0);
        lv_obj_set_style_border_color(b, border, 0);
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, tcol, 0);
        lv_obj_center(l);
    };
    make_nav_btn(  0, 120, lv_color_make(0x00,0x22,0x12), UI_COL_GREEN,
                   LV_SYMBOL_SETTINGS " CTRL",   UI_COL_GREEN,           cb_goto_temp);
    make_nav_btn(120, 120, lv_color_make(0x06,0x14,0x06), lv_color_make(0x80,0xFF,0x40),
                   LV_SYMBOL_IMAGE " GRAFICO",   lv_color_make(0x80,0xFF,0x40), cb_goto_graph);
    make_nav_btn(240, 120, lv_color_make(0x00,0x20,0x44), lv_color_make(0x00,0x80,0xFF),
                   LV_SYMBOL_WIFI " WiFi",       lv_color_make(0x00,0x80,0xFF), cb_goto_wifi);
    make_nav_btn(360, 120, lv_color_make(0x30,0x14,0x00), lv_color_make(0xFF,0x6B,0x00),
                   LV_SYMBOL_UPLOAD " OTA",      lv_color_make(0xFF,0x6B,0x00), cb_goto_ota);

    // ── Panel BASE  x=2 y=33 w=234 h=180 ──
    lv_obj_t* pb = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(pb, 2, 33); lv_obj_set_size(pb, 234, 180);
    lv_obj_set_style_bg_color(pb, lv_color_make(0x14,0x14,0x22), 0);
    lv_obj_set_style_bg_opa(pb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pb, UI_COL_ACCENT, 0);
    lv_obj_set_style_border_width(pb, 2, 0);
    lv_obj_set_style_radius(pb, 10, 0);
    lv_obj_set_style_pad_all(pb, 0, 0);
    lv_obj_clear_flag(pb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lnb = lv_label_create(pb);
    lv_label_set_text(lnb, "BASE");
    lv_obj_set_style_text_font(lnb, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lnb, UI_COL_ACCENT, 0);
    lv_obj_set_pos(lnb, 8, 6);

    ui_LedRelayBase = lv_led_create(pb);
    lv_led_set_color(ui_LedRelayBase, UI_COL_ACCENT);
    lv_obj_set_size(ui_LedRelayBase, 14, 14);
    lv_obj_set_pos(ui_LedRelayBase, 56, 7);
    lv_led_off(ui_LedRelayBase);

    // Pannello h=180, pad=0
    // Label "BASE" h≈20 → area temp da y=28 a y=138 (sopra barra)
    // Centro area = (28+138)/2 = 83. font48 ascender≈52px → y_top=83-26=57
    ui_LabelTempBase = lv_label_create(pb);
    lv_label_set_text(ui_LabelTempBase, "---");
    lv_obj_set_style_text_font(ui_LabelTempBase, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_LabelTempBase, UI_COL_WHITE, 0);
    lv_obj_align(ui_LabelTempBase, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* lu_b = lv_label_create(pb);
    lv_label_set_text(lu_b, "\xC2\xB0""C");
    lv_obj_set_style_text_font(lu_b, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lu_b, lv_color_make(0x70,0x70,0x90), 0);
    lv_obj_align(lu_b, LV_ALIGN_TOP_RIGHT, -6, 6);

    ui_LabelErrBase = lv_label_create(pb);
    lv_label_set_text(ui_LabelErrBase, "");
    lv_obj_set_style_text_color(ui_LabelErrBase, lv_color_make(0xFF,0x50,0x50), 0);
    lv_obj_set_style_text_font(ui_LabelErrBase, &lv_font_montserrat_14, 0);
    lv_obj_align(ui_LabelErrBase, LV_ALIGN_CENTER, 0, -20);

    // Barra PID y=138 h=14 | label PID y=153 h=14 → bottom=167, margine 13px
    ui_BarPidBase = make_bar(pb, 8, 138, 218, 14, UI_COL_ACCENT);
    ui_LabelPidBase = lv_label_create(pb);
    lv_label_set_text(ui_LabelPidBase, "PID 0%");
    lv_obj_set_style_text_font(ui_LabelPidBase, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelPidBase, UI_COL_GRAY, 0);
    lv_obj_set_pos(ui_LabelPidBase, 8, 153);

    // ── Panel CIELO  x=244 y=33 w=234 h=180 ──
    lv_obj_t* pc = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(pc, 244, 33); lv_obj_set_size(pc, 234, 180);
    lv_obj_set_style_bg_color(pc, lv_color_make(0x14,0x14,0x22), 0);
    lv_obj_set_style_bg_opa(pc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pc, UI_COL_CIELO, 0);
    lv_obj_set_style_border_width(pc, 2, 0);
    lv_obj_set_style_radius(pc, 10, 0);
    lv_obj_set_style_pad_all(pc, 0, 0);
    lv_obj_clear_flag(pc, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lnc = lv_label_create(pc);
    lv_label_set_text(lnc, "CIELO");
    lv_obj_set_style_text_font(lnc, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lnc, UI_COL_CIELO, 0);
    lv_obj_set_pos(lnc, 8, 6);

    ui_LedRelayCielo = lv_led_create(pc);
    lv_led_set_color(ui_LedRelayCielo, UI_COL_CIELO);
    lv_obj_set_size(ui_LedRelayCielo, 14, 14);
    lv_obj_set_pos(ui_LedRelayCielo, 66, 7);
    lv_led_off(ui_LedRelayCielo);

    ui_LabelTempCielo = lv_label_create(pc);
    lv_label_set_text(ui_LabelTempCielo, "---");
    lv_obj_set_style_text_font(ui_LabelTempCielo, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_LabelTempCielo, UI_COL_WHITE, 0);
    lv_obj_align(ui_LabelTempCielo, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* lu_c = lv_label_create(pc);
    lv_label_set_text(lu_c, "\xC2\xB0""C");
    lv_obj_set_style_text_font(lu_c, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lu_c, lv_color_make(0x70,0x70,0x90), 0);
    lv_obj_align(lu_c, LV_ALIGN_TOP_RIGHT, -6, 6);

    ui_LabelErrCielo = lv_label_create(pc);
    lv_label_set_text(ui_LabelErrCielo, "");
    lv_obj_set_style_text_color(ui_LabelErrCielo, lv_color_make(0xFF,0x50,0x50), 0);
    lv_obj_set_style_text_font(ui_LabelErrCielo, &lv_font_montserrat_14, 0);
    lv_obj_align(ui_LabelErrCielo, LV_ALIGN_CENTER, 0, -20);

    ui_BarPidCielo = make_bar(pc, 8, 138, 218, 14, UI_COL_CIELO);
    ui_LabelPidCielo = lv_label_create(pc);
    lv_label_set_text(ui_LabelPidCielo, "PID 0%");
    lv_obj_set_style_text_font(ui_LabelPidCielo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelPidCielo, UI_COL_GRAY, 0);
    lv_obj_set_pos(ui_LabelPidCielo, 8, 153);

    // ── Sep y=213 ──
    make_sep(ui_ScreenMain, 213, lv_color_make(0x30,0x30,0x48));

    // ── ON/OFF bar  y=215 h=33 ──
    // BASE x=2 w=156 | CIELO x=160 w=156 | LUCE x=318 w=160
    ui_BtnEnBase  = make_ctrl(ui_ScreenMain,   2, 215, 156, 33,
        "BASE OFF",  cb_toggle_base,  &ui_LabelBtnBase);
    ui_BtnEnCielo = make_ctrl(ui_ScreenMain, 160, 215, 156, 33,
        "CIELO OFF", cb_toggle_cielo, &ui_LabelBtnCielo);
    ui_BtnLuce    = make_ctrl(ui_ScreenMain, 318, 215, 160, 33,
        LV_SYMBOL_CHARGE " LUCE OFF", cb_toggle_luce, &ui_LabelBtnLuce);

    // ── Status bar y=249 h=23 ──
    lv_obj_t* sb = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(sb, 0, 249); lv_obj_set_size(sb, SCR_W, 23);
    lv_obj_set_style_bg_color(sb, lv_color_make(0x06,0x06,0x0E), 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    ui_LedFan = lv_led_create(sb);
    lv_led_set_color(ui_LedFan, lv_color_make(0x00,0xFF,0xCC));
    lv_obj_set_size(ui_LedFan, 10, 10);
    lv_obj_align(ui_LedFan, LV_ALIGN_RIGHT_MID, -40, 0);
    lv_led_off(ui_LedFan);

    lv_obj_t* lfan = lv_label_create(sb);
    lv_label_set_text(lfan, "FAN");
    lv_obj_set_style_text_font(lfan, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lfan, lv_color_make(0x40,0x40,0x55), 0);
    lv_obj_align(lfan, LV_ALIGN_RIGHT_MID, -4, 0);

    ui_LabelStatus = lv_label_create(sb);
    lv_label_set_text(ui_LabelStatus, "Sistema pronto");
    lv_obj_set_style_text_font(ui_LabelStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_GRAY, 0);
    lv_obj_align(ui_LabelStatus, LV_ALIGN_LEFT_MID, 6, 0);

    // Safety banner — sovrapposto ai pannelli
    ui_PanelSafety = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(ui_PanelSafety, 0, 33); lv_obj_set_size(ui_PanelSafety, SCR_W, 36);
    lv_obj_set_style_bg_color(ui_PanelSafety, lv_color_make(0xC0,0x00,0x00), 0);
    lv_obj_set_style_bg_opa(ui_PanelSafety, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_PanelSafety, 0, 0);
    lv_obj_set_style_radius(ui_PanelSafety, 0, 0);
    lv_obj_set_style_pad_all(ui_PanelSafety, 4, 0);
    lv_obj_clear_flag(ui_PanelSafety, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_PanelSafety, LV_OBJ_FLAG_HIDDEN);
    ui_LabelSafety = lv_label_create(ui_PanelSafety);
    lv_label_set_text(ui_LabelSafety, LV_SYMBOL_WARNING "  SHUTDOWN SICUREZZA");
    lv_obj_set_style_text_font(ui_LabelSafety, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui_LabelSafety, lv_color_white(), 0);
    lv_obj_center(ui_LabelSafety);
}

// ================================================================
//  BUILD TEMP/CTRL — layout rivisitato
// ================================================================
// y=0..31    h=32  Riga ON/OFF in cima: BASE|CIELO|LUCE|←MAIN
// y=33..182  h=150 pannelli BASE + CIELO (arco+setpoint+±+barra)
// y=183      sep verde
// y=184..208 h=25  header "⚙ CONTROLLO SETPOINT" + ←OK
// y=209..234 h=26  nav: RICETTE | TIMER | PID
// y=235      sep viola
// y=236..269 h=34  MODE toggle + PanelSplit
// (totale usato: 270px su 272 disponibili)
// ================================================================
static void build_temp() {
    ui_ScreenTemp = make_screen(UI_COL_BG);

    // ── Riga ON/OFF in cima  y=0 h=32 ──
    // BASE(116) | CIELO(116) | LUCE(80) | ←MAIN(166)  = 478px + gap
    ui_TempBtnEnBase  = make_ctrl(ui_ScreenTemp,   1, 1, 115, 30,
        "BASE OFF", cb_toggle_base, &ui_TempLblBtnBase);
    ui_TempBtnEnCielo = make_ctrl(ui_ScreenTemp, 118, 1, 115, 30,
        "CIELO OFF", cb_toggle_cielo, &ui_TempLblBtnCielo);
    ui_TempBtnLuce    = make_ctrl(ui_ScreenTemp, 235, 1, 78, 30,
        LV_SYMBOL_CHARGE " LUCE", cb_toggle_luce, &ui_TempLblBtnLuce);

    // ← MAIN
    lv_obj_t* bmn = lv_btn_create(ui_ScreenTemp);
    lv_obj_set_pos(bmn, 315, 1); lv_obj_set_size(bmn, 164, 30);
    lv_obj_set_style_bg_color(bmn, lv_color_make(0x00,0x20,0x0A), 0);
    lv_obj_set_style_border_color(bmn, UI_COL_GREEN, 0);
    lv_obj_set_style_border_width(bmn, 2, 0);
    lv_obj_set_style_radius(bmn, 8, 0);
    lv_obj_set_style_shadow_width(bmn, 0, 0);
    lv_obj_add_event_cb(bmn, cb_goto_main_from_temp, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lmn = lv_label_create(bmn);
    lv_label_set_text(lmn, LV_SYMBOL_LEFT " TORNA MAIN");
    lv_obj_set_style_text_font(lmn, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lmn, UI_COL_GREEN, 0);
    lv_obj_center(lmn);

    // ── Panel BASE  x=2 y=33 w=234 h=150 ──
    // Interno (pad=6): 222×138px
    // Arco 112×112: x=0 y=14 → centro y=70
    // Setpoint: y=57 h=26 centrato sull'arco, w=108 text_align=CENTER
    // Bottoni: x=114 w=108 h=52  y+=14  y-=70  gap=4px
    // Barra PID: y=128 h=8
    lv_obj_t* pb = lv_obj_create(ui_ScreenTemp);
    lv_obj_set_pos(pb, 2, 33); lv_obj_set_size(pb, 234, 150);
    lv_obj_set_style_bg_color(pb, lv_color_make(0x14,0x14,0x22), 0);
    lv_obj_set_style_bg_opa(pb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pb, UI_COL_ACCENT, 0);
    lv_obj_set_style_border_width(pb, 2, 0);
    lv_obj_set_style_radius(pb, 10, 0);
    lv_obj_set_style_pad_all(pb, 6, 0);
    lv_obj_clear_flag(pb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lb = lv_label_create(pb);
    lv_label_set_text(lb, "BASE");
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lb, UI_COL_ACCENT, 0);
    lv_obj_set_pos(lb, 0, 0);

    ui_ArcBase = make_arc(pb, 0, 14, 112, UI_COL_ACCENT);

    // Setpoint centrato sull'arco con align (arco è child di pb, setpoint pure)
    // L'arco occupa pb-local x=0 y=14 w=112 h=112 → centro locale = (56, 70)
    ui_TempSetBase = lv_label_create(pb);
    lv_label_set_text(ui_TempSetBase, "250\xC2\xB0""C");
    lv_obj_set_style_text_font(ui_TempSetBase, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ui_TempSetBase, UI_COL_YELLOW, 0);
    lv_obj_set_pos(ui_TempSetBase, 0, 56);
    lv_obj_set_size(ui_TempSetBase, 112, 28);
    lv_obj_set_style_text_align(ui_TempSetBase, LV_TEXT_ALIGN_CENTER, 0);

    make_pm(pb, 114, 14, 108, 52, "+", UI_COL_ACCENT, cb_base_plus);
    make_pm(pb, 114, 70, 108, 52, "-", UI_COL_ACCENT, cb_base_minus);

    lv_obj_t* sbl = lv_label_create(pb);
    lv_label_set_text(sbl, "\xC2\xB1""5\xC2\xB0""C");
    lv_obj_set_style_text_font(sbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(sbl, UI_COL_GRAY, 0);
    lv_obj_set_pos(sbl, 152, 124);

    ui_TempBarBase = make_bar(pb, 0, 128, 222, 8, UI_COL_ACCENT);

    ui_TempErrBase = lv_label_create(pb);
    lv_label_set_text(ui_TempErrBase, "");
    lv_obj_set_style_text_color(ui_TempErrBase, lv_color_make(0xFF,0x50,0x50), 0);
    lv_obj_set_style_text_font(ui_TempErrBase, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(ui_TempErrBase, 0, 57);
    lv_obj_set_size(ui_TempErrBase, 112, 26);
    lv_obj_set_style_text_align(ui_TempErrBase, LV_TEXT_ALIGN_CENTER, 0);

    // ── Panel CIELO  x=244 y=33 w=234 h=150 ──
    lv_obj_t* pcc = lv_obj_create(ui_ScreenTemp);
    lv_obj_set_pos(pcc, 244, 33); lv_obj_set_size(pcc, 234, 150);
    lv_obj_set_style_bg_color(pcc, lv_color_make(0x14,0x14,0x22), 0);
    lv_obj_set_style_bg_opa(pcc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pcc, UI_COL_CIELO, 0);
    lv_obj_set_style_border_width(pcc, 2, 0);
    lv_obj_set_style_radius(pcc, 10, 0);
    lv_obj_set_style_pad_all(pcc, 6, 0);
    lv_obj_clear_flag(pcc, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lc = lv_label_create(pcc);
    lv_label_set_text(lc, "CIELO");
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lc, UI_COL_CIELO, 0);
    lv_obj_set_pos(lc, 0, 0);

    ui_ArcCielo = make_arc(pcc, 0, 14, 112, UI_COL_CIELO);

    ui_TempSetCielo = lv_label_create(pcc);
    lv_label_set_text(ui_TempSetCielo, "300\xC2\xB0""C");
    lv_obj_set_style_text_font(ui_TempSetCielo, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ui_TempSetCielo, UI_COL_YELLOW, 0);
    lv_obj_set_pos(ui_TempSetCielo, 0, 56);
    lv_obj_set_size(ui_TempSetCielo, 112, 28);
    lv_obj_set_style_text_align(ui_TempSetCielo, LV_TEXT_ALIGN_CENTER, 0);

    make_pm(pcc, 114, 14, 108, 52, "+", UI_COL_CIELO, cb_cielo_plus);
    make_pm(pcc, 114, 70, 108, 52, "-", UI_COL_CIELO, cb_cielo_minus);

    lv_obj_t* scl = lv_label_create(pcc);
    lv_label_set_text(scl, "\xC2\xB1""5\xC2\xB0""C");
    lv_obj_set_style_text_font(scl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(scl, UI_COL_GRAY, 0);
    lv_obj_set_pos(scl, 152, 124);

    ui_TempBarCielo = make_bar(pcc, 0, 128, 222, 8, UI_COL_CIELO);

    ui_TempErrCielo = lv_label_create(pcc);
    lv_label_set_text(ui_TempErrCielo, "");
    lv_obj_set_style_text_color(ui_TempErrCielo, lv_color_make(0xFF,0x50,0x50), 0);
    lv_obj_set_style_text_font(ui_TempErrCielo, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(ui_TempErrCielo, 0, 57);
    lv_obj_set_size(ui_TempErrCielo, 112, 26);
    lv_obj_set_style_text_align(ui_TempErrCielo, LV_TEXT_ALIGN_CENTER, 0);

    // ── Header verde rimosso: 27px recuperati redistribuiti su nav e mode ──
    // Nuova zona inferiore (y=183..271):
    //   Sep      y=183  h=2
    //   Nav      y=185  h=40  RICETTE | TIMER | PID  (w=158 × 3, gap 2)
    //   Sep      y=225  h=2
    //   Mode     y=227  h=43  SINGLE | PanelSplit
    //   bottom   y=270  ✓ entro 272px

    // ── Sep y=183 ──
    make_sep(ui_ScreenTemp, 183, lv_color_make(0x30,0x30,0x48));

    // ── Nav azioni y=185 h=40: RICETTE | TIMER | PID ──
    auto make_nav_action = [&](int x, lv_color_t bg, lv_color_t border,
                               const char* txt, lv_color_t tcol, lv_event_cb_t cb) {
        lv_obj_t* b = lv_btn_create(ui_ScreenTemp);
        lv_obj_set_pos(b, x, 185); lv_obj_set_size(b, 158, 40);
        lv_obj_set_style_bg_color(b, bg, 0);
        lv_obj_set_style_border_color(b, border, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, tcol, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(l, tcol, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);
    };
    make_nav_action(  2, lv_color_make(0x00,0x20,0x10), lv_color_make(0x00,0xFF,0x90),
                   LV_SYMBOL_LIST " RICETTE",  lv_color_make(0x00,0xFF,0x90), cb_goto_ricette);
    make_nav_action(162, lv_color_make(0x10,0x10,0x28), lv_color_make(0x80,0x80,0xFF),
                   LV_SYMBOL_LOOP " TIMER",    lv_color_make(0x80,0x80,0xFF), cb_goto_timer);
    make_nav_action(322, lv_color_make(0x00,0x14,0x24), UI_COL_TUNING,
                   LV_SYMBOL_SETTINGS " PID",  UI_COL_TUNING,                 cb_goto_pid_base);

    // ── Sep y=225 ──
    make_sep(ui_ScreenTemp, 225, UI_COL_SINGLE);

    // ── Mode toggle  x=2 y=227 w=150 h=43 ──
    ui_BtnModeToggle = lv_btn_create(ui_ScreenTemp);
    lv_obj_set_pos(ui_BtnModeToggle, 2, 227); lv_obj_set_size(ui_BtnModeToggle, 150, 43);
    lv_obj_set_style_bg_color(ui_BtnModeToggle, lv_color_make(0x18,0x00,0x28), 0);
    lv_obj_set_style_border_color(ui_BtnModeToggle, UI_COL_SINGLE, 0);
    lv_obj_set_style_border_width(ui_BtnModeToggle, 2, 0);
    lv_obj_set_style_radius(ui_BtnModeToggle, 8, 0);
    lv_obj_set_style_shadow_width(ui_BtnModeToggle, 0, 0);
    lv_obj_add_event_cb(ui_BtnModeToggle, cb_toggle_mode,      LV_EVENT_CLICKED,      NULL);
    lv_obj_add_event_cb(ui_BtnModeToggle, cb_mode_long_press,  LV_EVENT_LONG_PRESSED, NULL);
    ui_LabelMode = lv_label_create(ui_BtnModeToggle);
    lv_label_set_text(ui_LabelMode, LV_SYMBOL_PAUSE " SINGLE");
    lv_obj_set_style_text_font(ui_LabelMode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelMode, UI_COL_SINGLE, 0);
    lv_obj_center(ui_LabelMode);

    // ── PanelSplit x=154 y=227 w=324 h=43 ──
    ui_PanelSplit = lv_obj_create(ui_ScreenTemp);
    lv_obj_set_pos(ui_PanelSplit, 154, 227); lv_obj_set_size(ui_PanelSplit, 324, 43);
    lv_obj_set_style_bg_color(ui_PanelSplit, lv_color_make(0x12,0x00,0x22), 0);
    lv_obj_set_style_border_color(ui_PanelSplit, UI_COL_SINGLE, 0);
    lv_obj_set_style_border_width(ui_PanelSplit, 1, 0);
    lv_obj_set_style_radius(ui_PanelSplit, 8, 0);
    lv_obj_set_style_pad_all(ui_PanelSplit, 4, 0);
    lv_obj_clear_flag(ui_PanelSplit, LV_OBJ_FLAG_SCROLLABLE);

    // BASE: label | - | +   (metà sinistra ~158px)  bottoni h=35 centrati in h=43
    ui_LabelPctBase = lv_label_create(ui_PanelSplit);
    lv_label_set_text(ui_LabelPctBase, "B 100%");
    lv_obj_set_style_text_font(ui_LabelPctBase, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelPctBase, UI_COL_ACCENT, 0);
    lv_obj_set_pos(ui_LabelPctBase, 2, 12);
    ui_BtnPctBaseMinus = make_pm(ui_PanelSplit, 66, 4, 40, 35, "-", UI_COL_ACCENT, cb_pct_base_minus);
    ui_BtnPctBasePlus  = make_pm(ui_PanelSplit, 110, 4, 40, 35, "+", UI_COL_ACCENT, cb_pct_base_plus);

    // Separatore verticale x=154
    lv_obj_t* vsep = lv_obj_create(ui_PanelSplit);
    lv_obj_set_pos(vsep, 154, 4); lv_obj_set_size(vsep, 2, 35);
    lv_obj_set_style_bg_color(vsep, UI_COL_SINGLE, 0);
    lv_obj_set_style_bg_opa(vsep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vsep, 0, 0);

    // CIELO: label | - | +   (metà destra)
    ui_LabelPctCielo = lv_label_create(ui_PanelSplit);
    lv_label_set_text(ui_LabelPctCielo, "C 100%");
    lv_obj_set_style_text_font(ui_LabelPctCielo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelPctCielo, UI_COL_CIELO, 0);
    lv_obj_set_pos(ui_LabelPctCielo, 160, 12);
    ui_BtnPctCieloMinus = make_pm(ui_PanelSplit, 224, 4, 40, 35, "-", UI_COL_CIELO, cb_pct_cielo_minus);
    ui_BtnPctCieloPlus  = make_pm(ui_PanelSplit, 268, 4, 40, 35, "+", UI_COL_CIELO, cb_pct_cielo_plus);

    lv_obj_add_flag(ui_PanelSplit, LV_OBJ_FLAG_HIDDEN);

    // Labels nascoste (compatibilità)
    ui_TempModeLbl = lv_label_create(ui_ScreenTemp);
    lv_label_set_text(ui_TempModeLbl, ""); lv_obj_add_flag(ui_TempModeLbl, LV_OBJ_FLAG_HIDDEN);
    ui_TempFanLbl = lv_label_create(ui_ScreenTemp);
    lv_label_set_text(ui_TempFanLbl, "");  lv_obj_add_flag(ui_TempFanLbl, LV_OBJ_FLAG_HIDDEN);
    ui_TempStatusLbl = lv_label_create(ui_ScreenTemp);
    lv_label_set_text(ui_TempStatusLbl, ""); lv_obj_add_flag(ui_TempStatusLbl, LV_OBJ_FLAG_HIDDEN);
    ui_TempSafetyLbl = lv_label_create(ui_ScreenTemp);
    lv_label_set_text(ui_TempSafetyLbl, ""); lv_obj_add_flag(ui_TempSafetyLbl, LV_OBJ_FLAG_HIDDEN);
    ui_LabelSplitCielo = lv_label_create(ui_ScreenTemp);
    lv_label_set_text(ui_LabelSplitCielo, ""); lv_obj_add_flag(ui_LabelSplitCielo, LV_OBJ_FLAG_HIDDEN);
    ui_BtnSplitMinus = lv_btn_create(ui_ScreenTemp); lv_obj_add_flag(ui_BtnSplitMinus, LV_OBJ_FLAG_HIDDEN);
    ui_BtnSplitPlus  = lv_btn_create(ui_ScreenTemp); lv_obj_add_flag(ui_BtnSplitPlus,  LV_OBJ_FLAG_HIDDEN);
}

// ================================================================
//  BUILD PID_BASE / PID_CIELO — parametri su tutta larghezza 480px
// ================================================================
// Layout 480×272:
//   Header      y=0..31    h=32  (colore + ← back | fwd →)
//   Sep         y=32
//   Riga Kp     y=33..104  h=72  (nome font32 | − 148×46 | valore font32 | + 148×46)
//   Sep         y=105
//   Riga Ki     y=106..177 h=72
//   Sep         y=178
//   Riga Kd     y=179..250 h=72
//   Footer      y=251..271 h=21
// ================================================================
static void build_pid_row(lv_obj_t* scr, int y, int h,
                          const char* name, lv_color_t col,
                          lv_event_cb_t cbm, lv_event_cb_t cbp,
                          lv_obj_t** val_out) {
    lv_obj_t* row = lv_obj_create(scr);
    lv_obj_set_pos(row, 0, y); lv_obj_set_size(row, SCR_W, h);
    lv_obj_set_style_bg_color(row, lv_color_make(0x12,0x12,0x1E), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Nome Kp/Ki/Kd — font32 — a sinistra centrato verticalmente
    lv_obj_t* lname = lv_label_create(row);
    lv_label_set_text(lname, name);
    lv_obj_set_style_text_font(lname, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lname, col, 0);
    lv_obj_align(lname, LV_ALIGN_LEFT_MID, 6, 0);

    // Bottone − x=56 w=148 h=48
    int bh = (h >= 54) ? 48 : (h - 8);
    int by = (h - bh) / 2;
    make_pm(row, 56, by, 148, bh, "-", col, cbm);

    // Valore font32 centrato
    lv_obj_t* val = lv_label_create(row);
    lv_label_set_text(val, "0.00");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(val, UI_COL_WHITE, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, 0);
    if (val_out) *val_out = val;

    // Bottone + x=276 w=148 h=48
    make_pm(row, 278, by, 148, bh, "+", col, cbp);
}

static void build_pid_screen(lv_obj_t** scr_out,
                              lv_color_t hdr_col, const char* hdr_txt,
                              const char* back_txt, lv_color_t back_col, lv_event_cb_t back_cb,
                              const char* fwd_txt,  lv_color_t fwd_col,  lv_event_cb_t fwd_cb,
                              lv_event_cb_t kp_m, lv_event_cb_t kp_p, lv_obj_t** kp_val,
                              lv_event_cb_t ki_m, lv_event_cb_t ki_p, lv_obj_t** ki_val,
                              lv_event_cb_t kd_m, lv_event_cb_t kd_p, lv_obj_t** kd_val,
                              lv_obj_t** set_lbl, lv_obj_t** save_status) {
    lv_obj_t* scr = make_screen(UI_COL_BG);
    *scr_out = scr;

    make_header(scr, hdr_col, hdr_txt);

    // Bottone ← (back)  x=2 y=2 w=90 h=28
    lv_obj_t* bbk = lv_btn_create(scr);
    lv_obj_set_pos(bbk, 2, 2); lv_obj_set_size(bbk, 90, 28);
    lv_obj_set_style_bg_color(bbk, back_col, 0);
    lv_obj_set_style_bg_opa(bbk, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bbk, 0, 0);
    lv_obj_set_style_radius(bbk, 6, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l1 = lv_label_create(bbk);
    lv_label_set_text(l1, back_txt);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_center(l1);

    // Bottone → (forward)  x=388 y=2 w=90 h=28
    lv_obj_t* bnxt = lv_btn_create(scr);
    lv_obj_set_pos(bnxt, 388, 2); lv_obj_set_size(bnxt, 90, 28);
    lv_obj_set_style_bg_color(bnxt, fwd_col, 0);
    lv_obj_set_style_bg_opa(bnxt, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bnxt, 0, 0);
    lv_obj_set_style_radius(bnxt, 6, 0);
    lv_obj_set_style_shadow_width(bnxt, 0, 0);
    lv_obj_add_event_cb(bnxt, fwd_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l2 = lv_label_create(bnxt);
    lv_label_set_text(l2, fwd_txt);
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);
    lv_obj_center(l2);

    make_sep(scr, 32, lv_color_make(0x40,0x40,0x60));

    build_pid_row(scr,  33, 72, "Kp", hdr_col, kp_m, kp_p, kp_val);
    make_sep(scr, 105, lv_color_make(0x24,0x24,0x38));
    build_pid_row(scr, 106, 72, "Ki", hdr_col, ki_m, ki_p, ki_val);
    make_sep(scr, 178, lv_color_make(0x24,0x24,0x38));
    build_pid_row(scr, 179, 72, "Kd", hdr_col, kd_m, kd_p, kd_val);

    // Footer  y=251..271 h=21
    lv_obj_t* ft = lv_obj_create(scr);
    lv_obj_set_pos(ft, 0, 251); lv_obj_set_size(ft, SCR_W, 21);
    lv_obj_set_style_bg_color(ft, lv_color_make(0x08,0x08,0x14), 0);
    lv_obj_set_style_bg_opa(ft, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ft, 0, 0);
    lv_obj_set_style_radius(ft, 0, 0);
    lv_obj_set_style_pad_all(ft, 0, 0);
    lv_obj_clear_flag(ft, LV_OBJ_FLAG_SCROLLABLE);

    *set_lbl = lv_label_create(ft);
    lv_label_set_text(*set_lbl, "SET: ---\xC2\xB0""C");
    lv_obj_set_style_text_font(*set_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*set_lbl, UI_COL_YELLOW, 0);
    lv_obj_align(*set_lbl, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t* bsv = lv_btn_create(ft);
    lv_obj_set_pos(bsv, 280, 2); lv_obj_set_size(bsv, 198, 17);
    lv_obj_set_style_bg_color(bsv, lv_color_make(0x00,0x28,0x16), 0);
    lv_obj_set_style_border_color(bsv, UI_COL_GREEN, 0);
    lv_obj_set_style_border_width(bsv, 1, 0);
    lv_obj_set_style_radius(bsv, 4, 0);
    lv_obj_set_style_shadow_width(bsv, 0, 0);
    lv_obj_add_event_cb(bsv, cb_pid_save, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lsv = lv_label_create(bsv);
    lv_label_set_text(lsv, LV_SYMBOL_SAVE " Salva Flash");
    lv_obj_set_style_text_font(lsv, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lsv, UI_COL_GREEN, 0);
    lv_obj_center(lsv);

    *save_status = lv_label_create(scr);
    lv_label_set_text(*save_status, "");
    lv_obj_set_style_text_font(*save_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(*save_status, UI_COL_GREEN, 0);
    lv_obj_add_flag(*save_status, LV_OBJ_FLAG_HIDDEN);
}

static void build_pid_base() {
    build_pid_screen(&ui_ScreenPidBase,
        UI_COL_ACCENT, LV_SYMBOL_SETTINGS "  PID — BASE",
        LV_SYMBOL_LEFT " CTRL",  lv_color_make(0x00,0x60,0x30), cb_goto_temp,
        "CIELO " LV_SYMBOL_RIGHT, UI_COL_CIELO,                  cb_goto_pid_cielo,
        cb_kp_base_m, cb_kp_base_p, &ui_LblKpBase,
        cb_ki_base_m, cb_ki_base_p, &ui_LblKiBase,
        cb_kd_base_m, cb_kd_base_p, &ui_LblKdBase,
        &ui_LblSetBaseTun, &ui_LblSaveStatus);
}

static void build_pid_cielo() {
    build_pid_screen(&ui_ScreenPidCielo,
        UI_COL_CIELO, LV_SYMBOL_SETTINGS "  PID — CIELO",
        LV_SYMBOL_LEFT " BASE",  UI_COL_ACCENT,                   cb_goto_pid_base,
        "CTRL " LV_SYMBOL_RIGHT, lv_color_make(0x00,0x60,0x30),   cb_goto_temp,
        cb_kp_cielo_m, cb_kp_cielo_p, &ui_LblKpCielo,
        cb_ki_cielo_m, cb_ki_cielo_p, &ui_LblKiCielo,
        cb_kd_cielo_m, cb_kd_cielo_p, &ui_LblKdCielo,
        &ui_LblSetCieloTun, &ui_LblSaveStatusCielo);
}

// ================================================================
//  BUILD GRAPH — chart 448×192 con scala Y e preset
// ================================================================
// Layout 480×272:
//   Header/nav  y=0..31    h=32
//   Chart       x=32 y=32  w=446 h=192 → bottom=224
//   Scale Y     x=0        labels verticali
//   Sep         y=225
//   Preset bar  y=226..271 h=46 — 3 bottoni 156px
// ================================================================
static void build_graph() {
    #define GLIME lv_color_make(0x80,0xFF,0x40)
    #define GBG   lv_color_make(0x05,0x09,0x05)

    ui_ScreenGraph = make_screen(UI_COL_BG);

    // Header
    lv_obj_t* hdr = lv_obj_create(ui_ScreenGraph);
    lv_obj_set_pos(hdr, 0, 0); lv_obj_set_size(hdr, SCR_W, 32);
    lv_obj_set_style_bg_color(hdr, GLIME, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lh = lv_label_create(hdr);
    lv_label_set_text(lh, LV_SYMBOL_IMAGE " GRAFICO TEMPERATURE");
    lv_obj_set_style_text_font(lh, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lh, lv_color_black(), 0);
    lv_obj_align(lh, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t* leg_b = lv_label_create(hdr);
    lv_label_set_text(leg_b, "— BASE");
    lv_obj_set_style_text_font(leg_b, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(leg_b, lv_color_make(0xFF,0x6B,0x00), 0);
    lv_obj_align(leg_b, LV_ALIGN_RIGHT_MID, -116, 0);

    lv_obj_t* leg_c = lv_label_create(hdr);
    lv_label_set_text(leg_c, "— CIELO");
    lv_obj_set_style_text_font(leg_c, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(leg_c, lv_color_make(0xFF,0x30,0x30), 0);
    lv_obj_align(leg_c, LV_ALIGN_RIGHT_MID, -2, 0);

    // ← OK
    lv_obj_t* bbk = lv_btn_create(ui_ScreenGraph);
    lv_obj_set_pos(bbk, 208, 2); lv_obj_set_size(bbk, 64, 28);
    lv_obj_set_style_bg_color(bbk, lv_color_make(0x30,0x60,0x10), 0);
    lv_obj_set_style_bg_opa(bbk, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bbk, 0, 0);
    lv_obj_set_style_radius(bbk, 6, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, cb_goto_main_from_graph, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbk = lv_label_create(bbk);
    lv_label_set_text(lbk, LV_SYMBOL_LEFT " OK");
    lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbk, lv_color_black(), 0);
    lv_obj_center(lbk);

    // Scala Y labels x=0 — 5 valori: 500/375/250/125/0
    // chart: y=32 h=192 → posizioni proporzionali
    static const char* yl[] = {"500","375","250","125","0"};
    static const int   yp[] = {32, 78, 124, 170, 214};
    for (int i = 0; i < 5; i++) {
        lv_obj_t* e = lv_label_create(ui_ScreenGraph);
        lv_label_set_text(e, yl[i]);
        lv_obj_set_style_text_font(e, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(e, lv_color_make(0x60,0x78,0x60), 0);
        lv_obj_set_pos(e, 2, yp[i]);
    }
    ui_GraphMaxLbl = lv_label_create(ui_ScreenGraph); lv_obj_add_flag(ui_GraphMaxLbl, LV_OBJ_FLAG_HIDDEN);
    ui_GraphMinLbl = lv_label_create(ui_ScreenGraph); lv_obj_add_flag(ui_GraphMinLbl, LV_OBJ_FLAG_HIDDEN);
    ui_GraphTimeLbl = lv_label_create(ui_ScreenGraph);
    lv_label_set_text(ui_GraphTimeLbl, "15 min");
    lv_obj_add_flag(ui_GraphTimeLbl, LV_OBJ_FLAG_HIDDEN);

    // Chart  x=32 y=32 w=446 h=192
    ui_Chart = lv_chart_create(ui_ScreenGraph);
    lv_obj_set_pos(ui_Chart, 32, 32); lv_obj_set_size(ui_Chart, 446, 192);
    lv_chart_set_type(ui_Chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(ui_Chart, LV_CHART_AXIS_PRIMARY_Y, 0, 500);
    lv_chart_set_div_line_count(ui_Chart, 4, 5);
    lv_chart_set_point_count(ui_Chart, GRAPH_BUF_SIZE);
    lv_obj_set_style_bg_color(ui_Chart, GBG, 0);
    lv_obj_set_style_bg_opa(ui_Chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ui_Chart, lv_color_make(0x28,0x44,0x28), 0);
    lv_obj_set_style_border_width(ui_Chart, 1, 0);
    lv_obj_set_style_radius(ui_Chart, 4, 0);
    lv_obj_set_style_line_color(ui_Chart, lv_color_make(0x18,0x34,0x18), 0);
    lv_obj_set_style_line_width(ui_Chart, 1, 0);
    lv_obj_clear_flag(ui_Chart, LV_OBJ_FLAG_CLICKABLE);
    ui_SerBase  = lv_chart_add_series(ui_Chart, lv_color_make(0xFF,0x6B,0x00), LV_CHART_AXIS_PRIMARY_Y);
    ui_SerCielo = lv_chart_add_series(ui_Chart, lv_color_make(0xFF,0x30,0x30), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_size(ui_Chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(ui_Chart, 2, LV_PART_ITEMS);
    lv_chart_set_all_value(ui_Chart, ui_SerBase,  LV_CHART_POINT_NONE);
    lv_chart_set_all_value(ui_Chart, ui_SerCielo, LV_CHART_POINT_NONE);

    // Sep y=225
    make_sep(ui_ScreenGraph, 225, GLIME);

    // 3 preset buttons  y=227 h=44  w=156 ciascuno con gap 2px
    auto mk_preset = [](lv_obj_t* par, int x, const char* txt, lv_event_cb_t cb) {
        lv_obj_t* b = lv_btn_create(par);
        lv_obj_set_pos(b, x, 227); lv_obj_set_size(b, 156, 44);
        lv_obj_set_style_bg_color(b, lv_color_make(0x0C,0x1C,0x0C), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(b, GLIME, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(l, GLIME, 0);
        lv_obj_center(l);
        return b;
    };
    ui_BtnPreset5  = mk_preset(ui_ScreenGraph,   2, "5 min",  cb_preset_5);
    ui_BtnPreset15 = mk_preset(ui_ScreenGraph, 162, "15 min", cb_preset_15);
    ui_BtnPreset30 = mk_preset(ui_ScreenGraph, 322, "30 min", cb_preset_30);
}

// ================================================================
//  BUILD TIMER — countdown cottura
// ================================================================
// Layout 480×272:
//   Header   y=0..31   h=32  (blu + ← CTRL)
//   Display  y=36..130 h=95  valore MM:SS font48 centrato (card w=472)
//   Riga ±   y=136..191 h=56 [ − 5 min ] [ + 5 min ] w=234 ciascuno
//   Riga ±1  y=194..236 h=43 [ − 15 min] [ + 15 min] w=234 ciascuno
//   RESET    y=239..263 h=25 tutta larghezza
//   Status   y=265..271 h=7  label sotto
// ================================================================
#define TIMER_COL  lv_color_make(0x80,0x80,0xFF)

static void build_timer() {
    ui_ScreenTimer = make_screen(UI_COL_BG);
    make_header(ui_ScreenTimer, TIMER_COL, LV_SYMBOL_LOOP "  TIMER COTTURA");

    // ← CTRL
    lv_obj_t* bbk = lv_btn_create(ui_ScreenTimer);
    lv_obj_set_pos(bbk, 414, 2); lv_obj_set_size(bbk, 64, 28);
    lv_obj_set_style_bg_color(bbk, lv_color_make(0x20,0x20,0x50), 0);
    lv_obj_set_style_border_width(bbk, 0, 0);
    lv_obj_set_style_radius(bbk, 6, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, cb_goto_temp, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbk = lv_label_create(bbk);
    lv_label_set_text(lbk, LV_SYMBOL_LEFT " OK");
    lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbk, TIMER_COL, 0);
    lv_obj_center(lbk);

    // Display MM:SS — card y=34 h=88 (ridotta da 93 per fare spazio)
    lv_obj_t* disp = lv_obj_create(ui_ScreenTimer);
    lv_obj_set_pos(disp, 4, 34); lv_obj_set_size(disp, 472, 88);
    lv_obj_set_style_bg_color(disp, lv_color_make(0x08,0x08,0x18), 0);
    lv_obj_set_style_bg_opa(disp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(disp, TIMER_COL, 0);
    lv_obj_set_style_border_width(disp, 2, 0);
    lv_obj_set_style_radius(disp, 14, 0);
    lv_obj_set_style_pad_all(disp, 0, 0);
    lv_obj_clear_flag(disp, LV_OBJ_FLAG_SCROLLABLE);

    ui_LabelTimerValue = lv_label_create(disp);
    lv_label_set_text(ui_LabelTimerValue, "10:00");
    lv_obj_set_style_text_font(ui_LabelTimerValue, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_LabelTimerValue, TIMER_COL, 0);
    lv_obj_center(ui_LabelTimerValue);

    // Riga ±5 min  y=125 h=52
    make_pm(ui_ScreenTimer,   2, 125, 236, 52, "-5 min", TIMER_COL, cb_timer_minus);
    make_pm(ui_ScreenTimer, 242, 125, 236, 52, "+5 min", TIMER_COL, cb_timer_plus);

    // Riga ±15 min  y=181 h=42
    make_pm(ui_ScreenTimer,   2, 181, 236, 42, "-15 min", UI_COL_GRAY, cb_timer_minus15);
    make_pm(ui_ScreenTimer, 242, 181, 236, 42, "+15 min", UI_COL_GRAY, cb_timer_plus15);

    // RESET — tutta larghezza  y=227 h=30 → bottom=257
    lv_obj_t* brs = lv_btn_create(ui_ScreenTimer);
    lv_obj_set_pos(brs, 2, 227); lv_obj_set_size(brs, 476, 30);
    lv_obj_set_style_bg_color(brs, lv_color_make(0x24,0x10,0x00), 0);
    lv_obj_set_style_border_color(brs, UI_COL_ACCENT, 0);
    lv_obj_set_style_border_width(brs, 2, 0);
    lv_obj_set_style_radius(brs, 8, 0);
    lv_obj_set_style_shadow_width(brs, 0, 0);
    lv_obj_add_event_cb(brs, cb_timer_reset, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lrs = lv_label_create(brs);
    lv_label_set_text(lrs, LV_SYMBOL_STOP "  RESET");
    lv_obj_set_style_text_font(lrs, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lrs, UI_COL_ACCENT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lrs, UI_COL_ACCENT, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_center(lrs);

    // Status label y=259 → bottom=271
    ui_LabelTimerStatus = lv_label_create(ui_ScreenTimer);
    lv_label_set_text(ui_LabelTimerStatus, "");
    lv_obj_set_style_text_font(ui_LabelTimerStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelTimerStatus, UI_COL_GRAY, 0);
    lv_obj_set_pos(ui_LabelTimerStatus, 0, 259);
    lv_obj_set_width(ui_LabelTimerStatus, SCR_W);
    lv_obj_set_style_text_align(ui_LabelTimerStatus, LV_TEXT_ALIGN_CENTER, 0);
}

// ================================================================
//  BUILD RICETTE — griglia 2×2 con card più grandi
// ================================================================
// Layout 480×272:
//   Header   y=0..31   h=32  (verde teal + ← CTRL)
//   4 card: griglia 2×2
//     w=234 h=116 con gap 4px
//     [0] x=2   y=36 → Margherita
//     [1] x=244 y=36 → Diavola
//     [2] x=2   y=156 → Calzone
//     [3] x=244 y=156 → Bianca
// ================================================================
#define RICETTE_COL  lv_color_make(0x00,0xFF,0x90)

struct Ricetta { const char* nome; int set_base; int set_cielo; int timer_min; };
static const Ricetta g_ricette[] = {
    { "Margherita",  250, 300,  8 },
    { "Diavola",     260, 320,  9 },
    { "Calzone",     240, 280, 12 },
    { "Bianca",      255, 310,  8 },
};

static void build_ricette() {
    ui_ScreenRicette = make_screen(UI_COL_BG);
    make_header(ui_ScreenRicette, RICETTE_COL, LV_SYMBOL_LIST "  RICETTE");

    // ← CTRL
    lv_obj_t* bbk = lv_btn_create(ui_ScreenRicette);
    lv_obj_set_pos(bbk, 414, 2); lv_obj_set_size(bbk, 64, 28);
    lv_obj_set_style_bg_color(bbk, lv_color_make(0x00,0x28,0x14), 0);
    lv_obj_set_style_border_width(bbk, 0, 0);
    lv_obj_set_style_radius(bbk, 6, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, cb_goto_temp, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbk = lv_label_create(bbk);
    lv_label_set_text(lbk, LV_SYMBOL_LEFT " OK");
    lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbk, RICETTE_COL, 0);
    lv_obj_center(lbk);

    // Griglia 2×2 ricette — card 234×116
    static const int xs[] = {2, 244, 2, 244};
    static const int ys[] = {36, 36, 156, 156};
    for (int i = 0; i < 4; i++) {
        const Ricetta& r = g_ricette[i];
        lv_obj_t* card = lv_btn_create(ui_ScreenRicette);
        lv_obj_set_pos(card, xs[i], ys[i]); lv_obj_set_size(card, 234, 114);
        lv_obj_set_style_bg_color(card, lv_color_make(0x08,0x14,0x10), 0);
        lv_obj_set_style_border_color(card, RICETTE_COL, 0);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_add_event_cb(card, cb_ricetta_select, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Nome ricetta — in alto centrato
        lv_obj_t* ln = lv_label_create(card);
        lv_label_set_text(ln, r.nome);
        lv_obj_set_style_text_font(ln, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ln, RICETTE_COL, 0);
        lv_obj_set_style_text_align(ln, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(ln, 218);
        lv_obj_set_pos(ln, 0, 2);

        // CIELO ↑
        char cbuf[20];
        snprintf(cbuf, sizeof(cbuf), LV_SYMBOL_UP " %d\xC2\xB0""C", r.set_cielo);
        lv_obj_t* lc = lv_label_create(card);
        lv_label_set_text(lc, cbuf);
        lv_obj_set_style_text_font(lc, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lc, UI_COL_CIELO, 0);
        lv_obj_set_style_text_align(lc, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lc, 218);
        lv_obj_set_pos(lc, 0, 34);

        // BASE ↓
        char bbuf[20];
        snprintf(bbuf, sizeof(bbuf), LV_SYMBOL_DOWN " %d\xC2\xB0""C", r.set_base);
        lv_obj_t* lb2 = lv_label_create(card);
        lv_label_set_text(lb2, bbuf);
        lv_obj_set_style_text_font(lb2, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lb2, UI_COL_ACCENT, 0);
        lv_obj_set_style_text_align(lb2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lb2, 218);
        lv_obj_set_pos(lb2, 0, 64);
    }
}

// ================================================================
//  ui_init / ui_show_screen
// ================================================================
void ui_init(void) {
    build_main();
    build_temp();
    build_pid_base();
    build_pid_cielo();
    build_graph();
    build_timer();
    build_ricette();
    ui_build_wifi();
    ui_build_ota();
    lv_scr_load(ui_ScreenMain);
}

void ui_show_screen(Screen s) {
    g_state.active_screen = s;
    switch (s) {
        case Screen::MAIN:
            lv_scr_load_anim(ui_ScreenMain,     LV_SCR_LOAD_ANIM_MOVE_RIGHT,  200, 0, false); break;
        case Screen::TEMP:
            lv_scr_load_anim(ui_ScreenTemp,     LV_SCR_LOAD_ANIM_MOVE_LEFT,   200, 0, false); break;
        case Screen::PID_BASE:
            lv_scr_load_anim(ui_ScreenPidBase,  LV_SCR_LOAD_ANIM_MOVE_TOP,    200, 0, false); break;
        case Screen::PID_CIELO:
            lv_scr_load_anim(ui_ScreenPidCielo, LV_SCR_LOAD_ANIM_MOVE_LEFT,   200, 0, false); break;
        case Screen::GRAPH:
            lv_scr_load_anim(ui_ScreenGraph,    LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false); break;
        case Screen::WIFI_SCAN:
            ui_show_wifi_scan(); break;
        case Screen::WIFI_PWD:
            break;
        case Screen::OTA:
            ui_show_ota(); break;
        case Screen::TIMER:
            lv_scr_load_anim(ui_ScreenTimer,   LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false); break;
        case Screen::RICETTE:
            lv_scr_load_anim(ui_ScreenRicette, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false); break;
    }
}

// ================================================================
//  ui_refresh — aggiorna MAIN
// ================================================================
void ui_refresh(AppState* s) {
    if (!s) return;

    // Temperatura BASE
    if (s->tc_base_err) {
        lv_label_set_text(ui_LabelTempBase, "ERR");
        lv_obj_set_style_text_color(ui_LabelTempBase, lv_color_make(0xFF,0x40,0x40), 0);
        lv_label_set_text(ui_LabelErrBase, "TC ERR");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", s->temp_base);
        lv_label_set_text(ui_LabelTempBase, buf);
        lv_obj_set_style_text_color(ui_LabelTempBase,
            ui_temp_color(s->temp_base, s->set_base), 0);
        lv_label_set_text(ui_LabelErrBase, "");
    }

    // Temperatura CIELO
    if (s->tc_cielo_err) {
        lv_label_set_text(ui_LabelTempCielo, "ERR");
        lv_obj_set_style_text_color(ui_LabelTempCielo, lv_color_make(0xFF,0x40,0x40), 0);
        lv_label_set_text(ui_LabelErrCielo, "TC ERR");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", s->temp_cielo);
        lv_label_set_text(ui_LabelTempCielo, buf);
        lv_obj_set_style_text_color(ui_LabelTempCielo,
            ui_temp_color(s->temp_cielo, s->set_cielo), 0);
        lv_label_set_text(ui_LabelErrCielo, "");
    }

    // Barre PID + label
    {
        int v = (int)s->pid_out_base;
        lv_bar_set_value(ui_BarPidBase, v, LV_ANIM_OFF);
        char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
        lv_label_set_text(ui_LabelPidBase, buf);
    }
    {
        int v = (int)s->pid_out_cielo;
        lv_bar_set_value(ui_BarPidCielo, v, LV_ANIM_OFF);
        char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
        lv_label_set_text(ui_LabelPidCielo, buf);
    }

    // LED relay
    if (s->relay_base)  lv_led_on(ui_LedRelayBase);  else lv_led_off(ui_LedRelayBase);
    if (s->relay_cielo) lv_led_on(ui_LedRelayCielo); else lv_led_off(ui_LedRelayCielo);

    // Bottoni ON/OFF BASE
    if (s->base_enabled) {
        lv_obj_set_style_bg_color(ui_BtnEnBase, lv_color_make(0x28,0x10,0x00), 0);
        lv_obj_set_style_border_color(ui_BtnEnBase, UI_COL_ACCENT, 0);
        lv_label_set_text(ui_LabelBtnBase, s->preheat_base ? LV_SYMBOL_WARNING " PRERISCALDO" : "BASE ON");
        lv_obj_set_style_text_color(ui_LabelBtnBase, UI_COL_ACCENT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnBase, UI_COL_ACCENT, LV_PART_MAIN | LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_color(ui_BtnEnBase, lv_color_make(0x20,0x20,0x2E), 0);
        lv_obj_set_style_border_color(ui_BtnEnBase, lv_color_make(0x48,0x48,0x60), 0);
        lv_label_set_text(ui_LabelBtnBase, "BASE OFF");
        lv_color_t g = lv_color_make(0xA0,0xA0,0xB8);
        lv_obj_set_style_text_color(ui_LabelBtnBase, g, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnBase, g, LV_PART_MAIN | LV_STATE_PRESSED);
    }

    // Bottoni ON/OFF CIELO
    if (s->cielo_enabled) {
        lv_obj_set_style_bg_color(ui_BtnEnCielo, lv_color_make(0x28,0x08,0x08), 0);
        lv_obj_set_style_border_color(ui_BtnEnCielo, UI_COL_CIELO, 0);
        lv_label_set_text(ui_LabelBtnCielo, s->preheat_cielo ? LV_SYMBOL_WARNING " PRERISCALDO" : "CIELO ON");
        lv_obj_set_style_text_color(ui_LabelBtnCielo, UI_COL_CIELO, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnCielo, UI_COL_CIELO, LV_PART_MAIN | LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_color(ui_BtnEnCielo, lv_color_make(0x20,0x20,0x2E), 0);
        lv_obj_set_style_border_color(ui_BtnEnCielo, lv_color_make(0x48,0x48,0x60), 0);
        lv_label_set_text(ui_LabelBtnCielo, "CIELO OFF");
        lv_color_t g = lv_color_make(0xA0,0xA0,0xB8);
        lv_obj_set_style_text_color(ui_LabelBtnCielo, g, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnCielo, g, LV_PART_MAIN | LV_STATE_PRESSED);
    }

    // Bottone LUCE
    if (s->luce_on) {
        lv_obj_set_style_bg_color(ui_BtnLuce, lv_color_make(0x28,0x24,0x00), 0);
        lv_obj_set_style_border_color(ui_BtnLuce, UI_COL_LUCE, 0);
        lv_label_set_text(ui_LabelBtnLuce, LV_SYMBOL_CHARGE " LUCE ON");
        lv_obj_set_style_text_color(ui_LabelBtnLuce, UI_COL_LUCE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnLuce, UI_COL_LUCE, LV_PART_MAIN | LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_bg_color(ui_BtnLuce, lv_color_make(0x20,0x20,0x2E), 0);
        lv_obj_set_style_border_color(ui_BtnLuce, lv_color_make(0x48,0x48,0x60), 0);
        lv_label_set_text(ui_LabelBtnLuce, LV_SYMBOL_CHARGE " LUCE OFF");
        lv_color_t g = lv_color_make(0xA0,0xA0,0xB8);
        lv_obj_set_style_text_color(ui_LabelBtnLuce, g, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnLuce, g, LV_PART_MAIN | LV_STATE_PRESSED);
    }

    // Safety banner
    if (s->safety_shutdown) {
        lv_obj_clear_flag(ui_PanelSafety, LV_OBJ_FLAG_HIDDEN);
        const char* reason = "ERRORE SCONOSCIUTO";
        switch (s->safety_reason) {
            case SafetyReason::TC_ERROR:     reason = "SENSORE TC IN ERRORE";  break;
            case SafetyReason::OVERTEMP:     reason = "SOVRATEMPERATURA";       break;
            case SafetyReason::RUNAWAY_DOWN: reason = "RUNAWAY: DISCESA";       break;
            case SafetyReason::RUNAWAY_UP:   reason = "RUNAWAY: SALITA";        break;
            case SafetyReason::WDG_TIMEOUT:  reason = "WATCHDOG TIMEOUT";       break;
            default: break;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING "  SHUTDOWN — %s", reason);
        lv_label_set_text(ui_LabelSafety, buf);
    } else {
        lv_obj_add_flag(ui_PanelSafety, LV_OBJ_FLAG_HIDDEN);
    }

    // Status bar
    if (s->safety_shutdown) {
        lv_label_set_text(ui_LabelStatus, LV_SYMBOL_WARNING " SICUREZZA ATTIVA");
        lv_obj_set_style_text_color(ui_LabelStatus, lv_color_make(0xFF,0x40,0x00), 0);
    } else if (!s->base_enabled && !s->cielo_enabled) {
        lv_label_set_text(ui_LabelStatus, "Sistema pronto");
        lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_GRAY, 0);
    } else if (s->preheat_base || s->preheat_cielo) {
        lv_label_set_text(ui_LabelStatus, LV_SYMBOL_WARNING " Preriscaldo in corso...");
        lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_ACCENT2, 0);
    } else {
        char buf[48];
        snprintf(buf, sizeof(buf), "B:%.0f/%.0f°  C:%.0f/%.0f°",
            s->temp_base, s->set_base, s->temp_cielo, s->set_cielo);
        lv_label_set_text(ui_LabelStatus, buf);
        lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_GRAY, 0);
    }

    if (s->fan_on) lv_led_on(ui_LedFan); else lv_led_off(ui_LedFan);
}

// ================================================================
//  ui_refresh_temp — aggiorna schermata CTRL
// ================================================================
void ui_refresh_temp(AppState* s) {
    if (!s) return;

    // Archi setpoint
    lv_arc_set_value(ui_ArcBase,  (int)s->set_base);
    lv_arc_set_value(ui_ArcCielo, (int)s->set_cielo);

    // Etichette setpoint
    char buf[20];
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0""C", s->set_base);
    lv_label_set_text(ui_TempSetBase, buf);
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0""C", s->set_cielo);
    lv_label_set_text(ui_TempSetCielo, buf);

    // Barre PID
    lv_bar_set_value(ui_TempBarBase,  (int)s->pid_out_base,  LV_ANIM_OFF);
    lv_bar_set_value(ui_TempBarCielo, (int)s->pid_out_cielo, LV_ANIM_OFF);

    // Errori TC
    lv_label_set_text(ui_TempErrBase,  s->tc_base_err  ? "TC ERR" : "");
    lv_label_set_text(ui_TempErrCielo, s->tc_cielo_err ? "TC ERR" : "");

    // Bottoni ON/OFF nella CTRL
    auto upd_btn_ctrl = [](lv_obj_t* btn, lv_obj_t* lbl,
                           bool en, bool pre,
                           const char* name_on, const char* name_off,
                           lv_color_t col_on, lv_color_t col_off) {
        if (en) {
            lv_obj_set_style_border_color(btn, col_on, 0);
            lv_label_set_text(lbl, pre ? LV_SYMBOL_WARNING " PRE" : name_on);
            lv_obj_set_style_text_color(lbl, col_on, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl, col_on, LV_PART_MAIN | LV_STATE_PRESSED);
        } else {
            lv_obj_set_style_border_color(btn, col_off, 0);
            lv_label_set_text(lbl, name_off);
            lv_obj_set_style_text_color(lbl, col_off, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl, col_off, LV_PART_MAIN | LV_STATE_PRESSED);
        }
    };
    upd_btn_ctrl(ui_TempBtnEnBase,  ui_TempLblBtnBase,  s->base_enabled,  s->preheat_base,
                 "BASE ON",  "BASE OFF",  UI_COL_ACCENT, lv_color_make(0x60,0x60,0x80));
    upd_btn_ctrl(ui_TempBtnEnCielo, ui_TempLblBtnCielo, s->cielo_enabled, s->preheat_cielo,
                 "CIELO ON", "CIELO OFF", UI_COL_CIELO,  lv_color_make(0x60,0x60,0x80));

    // Luce
    if (s->luce_on) {
        lv_obj_set_style_border_color(ui_TempBtnLuce, UI_COL_LUCE, 0);
        lv_label_set_text(ui_TempLblBtnLuce, LV_SYMBOL_CHARGE " ON");
        lv_obj_set_style_text_color(ui_TempLblBtnLuce, UI_COL_LUCE, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_border_color(ui_TempBtnLuce, lv_color_make(0x60,0x60,0x80), 0);
        lv_label_set_text(ui_TempLblBtnLuce, LV_SYMBOL_CHARGE " OFF");
        lv_obj_set_style_text_color(ui_TempLblBtnLuce, lv_color_make(0xA0,0xA0,0xB8), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Mode label
    if (s->sensor_mode == SensorMode::SINGLE) {
        lv_label_set_text(ui_LabelMode, LV_SYMBOL_PAUSE " SINGLE");
        lv_obj_set_style_text_color(ui_LabelMode, UI_COL_SINGLE, 0);
        lv_obj_set_style_border_color(ui_BtnModeToggle, UI_COL_SINGLE, 0);
    } else {
        lv_label_set_text(ui_LabelMode, LV_SYMBOL_PLAY " DUAL");
        lv_obj_set_style_text_color(ui_LabelMode, UI_COL_DUAL, 0);
        lv_obj_set_style_border_color(ui_BtnModeToggle, UI_COL_DUAL, 0);
    }

    // Pct labels
    char pb[12], pc[12];
    snprintf(pb, sizeof(pb), "B %d%%", s->pct_base);
    snprintf(pc, sizeof(pc), "C %d%%", s->pct_cielo);
    lv_label_set_text(ui_LabelPctBase,  pb);
    lv_label_set_text(ui_LabelPctCielo, pc);
}

// ================================================================
//  ui_refresh_pid — aggiorna PID_BASE o PID_CIELO
// ================================================================
void ui_refresh_pid(AppState* s) {
    if (!s) return;
    char buf[16];

    // PID BASE
    snprintf(buf, sizeof(buf), "%.2f", s->kp_base);
    lv_label_set_text(ui_LblKpBase, buf);
    snprintf(buf, sizeof(buf), "%.3f", s->ki_base);
    lv_label_set_text(ui_LblKiBase, buf);
    snprintf(buf, sizeof(buf), "%.2f", s->kd_base);
    lv_label_set_text(ui_LblKdBase, buf);
    snprintf(buf, sizeof(buf), "SET: %.0f\xC2\xB0""C", s->set_base);
    lv_label_set_text(ui_LblSetBaseTun, buf);

    // PID CIELO
    snprintf(buf, sizeof(buf), "%.2f", s->kp_cielo);
    lv_label_set_text(ui_LblKpCielo, buf);
    snprintf(buf, sizeof(buf), "%.3f", s->ki_cielo);
    lv_label_set_text(ui_LblKiCielo, buf);
    snprintf(buf, sizeof(buf), "%.2f", s->kd_cielo);
    lv_label_set_text(ui_LblKdCielo, buf);
    snprintf(buf, sizeof(buf), "SET: %.0f\xC2\xB0""C", s->set_cielo);
    lv_label_set_text(ui_LblSetCieloTun, buf);
}

// ================================================================
//  ui_refresh_graph — aggiorna grafico storico
// ================================================================
void ui_refresh_graph(AppState* s) {
    if (!s || !ui_Chart) return;

    // Determina quanti campioni mostrare in base a g_graph_minutes
    int max_pts = (g_graph_minutes * 60) / GRAPH_SAMPLE_S;
    if (max_pts > GRAPH_BUF_SIZE) max_pts = GRAPH_BUF_SIZE;
    int count = (g_graph.count < max_pts) ? g_graph.count : max_pts;

    lv_chart_set_all_value(ui_Chart, ui_SerBase,  LV_CHART_POINT_NONE);
    lv_chart_set_all_value(ui_Chart, ui_SerCielo, LV_CHART_POINT_NONE);

    for (int i = 0; i < count; i++) {
        int idx = ((int)g_graph.head - count + i + GRAPH_BUF_SIZE) % GRAPH_BUF_SIZE;
        lv_chart_set_next_value(ui_Chart, ui_SerBase,  (lv_coord_t)g_graph.base[idx]);
        lv_chart_set_next_value(ui_Chart, ui_SerCielo, (lv_coord_t)g_graph.cielo[idx]);
    }
    lv_chart_refresh(ui_Chart);
}

// ================================================================
//  TIMER — funzioni helper
// ================================================================
static int g_timer_seconds = 600; // 10 min default
static bool g_timer_running_flag = false;

void ui_timer_auto_start(void) {
    g_timer_running_flag = true;
    if (ui_LabelTimerStatus)
        lv_label_set_text(ui_LabelTimerStatus, "Timer avviato automaticamente");
}

void ui_timer_tick_1s(void) {
    if (!g_timer_running_flag || g_timer_seconds <= 0) return;
    g_timer_seconds--;
    if (ui_LabelTimerValue) {
        char buf[8];
        int m = g_timer_seconds / 60;
        int sc = g_timer_seconds % 60;
        snprintf(buf, sizeof(buf), "%02d:%02d", m, sc);
        lv_label_set_text(ui_LabelTimerValue, buf);
    }
    if (g_timer_seconds == 0) {
        g_timer_running_flag = false;
        if (ui_LabelTimerStatus)
            lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_BELL " TEMPO SCADUTO!");
    }
}
