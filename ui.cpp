/**
 * ui.cpp — Forno Pizza Controller v21-S3 + ANIMAZIONI
 * ================================================================
 * Layout 480×272 — Display RGB565 parallelo
 * Include: ui_animations.h (nuovo file)
 * ================================================================
 */

#include "ui.h"
#include "ui_wifi.h"
#include "ui_animations.h"   // ← NUOVO: tutte le animazioni

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
lv_obj_t* ui_LabelSplitCielo = NULL;
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

// ── NUOVI WIDGET v22 ────────────────────────────────────────────
// Barre preheat (sottili, sovrapposte ai pannelli BASE/CIELO su MAIN)
lv_obj_t* ui_BarPreheatBase  = NULL;   // anim 14
lv_obj_t* ui_BarPreheatCielo = NULL;   // anim 14
// Barre autotune indeterminate (schermate PID)
lv_obj_t* ui_BarAutotune     = NULL;   // anim 19 (condivisa base+cielo)
// Toast notifica overlay
static lv_obj_t* s_toast_ref = NULL;   // anim 16

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
    // ← ANIMAZIONE 5: bounce al tocco
    lv_obj_add_event_cb(b, cb_anim_bounce, LV_EVENT_PRESSED, NULL);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_center(l);
    return b;
}

static lv_obj_t* make_ctrl(lv_obj_t* p, int x, int y, int w, int h,
                             const char* txt, lv_event_cb_t cb, lv_obj_t** lbl_out) {
    lv_obj_t* b = lv_btn_create(p);
    lv_obj_set_pos(b, x, y); lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_make(0x20,0x20,0x2E), 0);
    lv_obj_set_style_border_color(b, lv_color_make(0x48,0x48,0x60), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(l, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    if (lbl_out) *lbl_out = l;
    return b;
}

// ================================================================
//  BUILD MAIN
// ================================================================
static void build_main() {
    ui_ScreenMain = make_screen(UI_COL_BG);

    // NavBar y=0 h=32: CTRL | GRAF | WiFi | OTA
    auto make_nav = [](lv_obj_t* scr, int x, lv_color_t bg, lv_color_t border,
                       const char* txt, lv_color_t tcol, lv_event_cb_t cb) -> lv_obj_t* {
        lv_obj_t* b = lv_btn_create(scr);
        lv_obj_set_pos(b, x, 1); lv_obj_set_size(b, 118, 30);
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
        lv_obj_center(l);
        return b;
    };
    make_nav(ui_ScreenMain,   2, lv_color_make(0x14,0x14,0x22), UI_COL_ACCENT,
             LV_SYMBOL_SETTINGS " CTRL",  UI_COL_ACCENT,  cb_goto_temp);
    make_nav(ui_ScreenMain, 122, lv_color_make(0x05,0x09,0x05), lv_color_make(0x80,0xFF,0x40),
             LV_SYMBOL_IMAGE " GRAF",     lv_color_make(0x80,0xFF,0x40), cb_goto_graph);
    make_nav(ui_ScreenMain, 242, lv_color_make(0x00,0x10,0x20), UI_COL_CYAN,
             LV_SYMBOL_WIFI " WiFi",      UI_COL_CYAN,    cb_goto_wifi);
    make_nav(ui_ScreenMain, 362, lv_color_make(0x14,0x08,0x00), lv_color_make(0xFF,0x80,0x00),
             LV_SYMBOL_UPLOAD " OTA",     lv_color_make(0xFF,0x80,0x00), cb_goto_ota);

    // Sep y=33
    make_sep(ui_ScreenMain, 33, UI_COL_DARKGRAY);

    // Pannello BASE x=2 y=36 w=234 h=178
    lv_obj_t* pb = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(pb, 2, 36); lv_obj_set_size(pb, 234, 178);
    lv_obj_set_style_bg_color(pb, lv_color_make(0x14,0x0A,0x00), 0);
    lv_obj_set_style_border_color(pb, UI_COL_ACCENT, 0);
    lv_obj_set_style_border_width(pb, 2, 0);
    lv_obj_set_style_radius(pb, 10, 0);
    lv_obj_set_style_pad_all(pb, 6, 0);
    lv_obj_clear_flag(pb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbh = lv_label_create(pb); lv_label_set_text(lbh, "BASE");
    lv_obj_set_style_text_font(lbh, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbh, UI_COL_ACCENT, 0); lv_obj_set_pos(lbh, 2, 0);

    ui_LabelTempBase = lv_label_create(pb); lv_label_set_text(ui_LabelTempBase, "---");
    lv_obj_set_style_text_font(ui_LabelTempBase, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_LabelTempBase, UI_COL_ACCENT, 0);
    lv_obj_set_pos(ui_LabelTempBase, 0, 22);
    lv_obj_set_width(ui_LabelTempBase, 222);
    lv_obj_set_style_text_align(ui_LabelTempBase, LV_TEXT_ALIGN_CENTER, 0);

    ui_LabelErrBase = lv_label_create(pb); lv_label_set_text(ui_LabelErrBase, "");
    lv_obj_set_style_text_font(ui_LabelErrBase, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelErrBase, lv_color_make(0xFF,0x40,0x40), 0);
    lv_obj_set_pos(ui_LabelErrBase, 0, 80);
    lv_obj_set_width(ui_LabelErrBase, 222);
    lv_obj_set_style_text_align(ui_LabelErrBase, LV_TEXT_ALIGN_CENTER, 0);

    ui_LedRelayBase = lv_led_create(pb);
    lv_obj_set_pos(ui_LedRelayBase, 192, 0); lv_obj_set_size(ui_LedRelayBase, 20, 20);
    lv_led_set_color(ui_LedRelayBase, UI_COL_ACCENT); lv_led_off(ui_LedRelayBase);

    ui_BarPidBase = make_bar(pb, 0, 96, 222, 8, UI_COL_ACCENT);
    ui_LabelPidBase = lv_label_create(pb); lv_label_set_text(ui_LabelPidBase, "PID 0%");
    lv_obj_set_style_text_font(ui_LabelPidBase, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelPidBase, UI_COL_GRAY, 0);
    lv_obj_set_pos(ui_LabelPidBase, 0, 106);

    // Pannello CIELO x=244 y=36 w=234 h=178
    lv_obj_t* pc = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(pc, 244, 36); lv_obj_set_size(pc, 234, 178);
    lv_obj_set_style_bg_color(pc, lv_color_make(0x14,0x00,0x00), 0);
    lv_obj_set_style_border_color(pc, UI_COL_CIELO, 0);
    lv_obj_set_style_border_width(pc, 2, 0);
    lv_obj_set_style_radius(pc, 10, 0);
    lv_obj_set_style_pad_all(pc, 6, 0);
    lv_obj_clear_flag(pc, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lch = lv_label_create(pc); lv_label_set_text(lch, "CIELO");
    lv_obj_set_style_text_font(lch, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lch, UI_COL_CIELO, 0); lv_obj_set_pos(lch, 2, 0);

    ui_LabelTempCielo = lv_label_create(pc); lv_label_set_text(ui_LabelTempCielo, "---");
    lv_obj_set_style_text_font(ui_LabelTempCielo, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_LabelTempCielo, UI_COL_CIELO, 0);
    lv_obj_set_pos(ui_LabelTempCielo, 0, 22);
    lv_obj_set_width(ui_LabelTempCielo, 222);
    lv_obj_set_style_text_align(ui_LabelTempCielo, LV_TEXT_ALIGN_CENTER, 0);

    ui_LabelErrCielo = lv_label_create(pc); lv_label_set_text(ui_LabelErrCielo, "");
    lv_obj_set_style_text_font(ui_LabelErrCielo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelErrCielo, lv_color_make(0xFF,0x40,0x40), 0);
    lv_obj_set_pos(ui_LabelErrCielo, 0, 80);
    lv_obj_set_width(ui_LabelErrCielo, 222);
    lv_obj_set_style_text_align(ui_LabelErrCielo, LV_TEXT_ALIGN_CENTER, 0);

    ui_LedRelayCielo = lv_led_create(pc);
    lv_obj_set_pos(ui_LedRelayCielo, 192, 0); lv_obj_set_size(ui_LedRelayCielo, 20, 20);
    lv_led_set_color(ui_LedRelayCielo, UI_COL_CIELO); lv_led_off(ui_LedRelayCielo);

    ui_BarPidCielo = make_bar(pc, 0, 96, 222, 8, UI_COL_CIELO);
    ui_LabelPidCielo = lv_label_create(pc); lv_label_set_text(ui_LabelPidCielo, "PID 0%");
    lv_obj_set_style_text_font(ui_LabelPidCielo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelPidCielo, UI_COL_GRAY, 0);
    lv_obj_set_pos(ui_LabelPidCielo, 0, 106);

    // Sep y=215
    make_sep(ui_ScreenMain, 215, UI_COL_DARKGRAY);

    // ON/OFF bar y=217 h=31
    ui_BtnEnBase  = lv_btn_create(ui_ScreenMain);
    lv_obj_set_pos(ui_BtnEnBase,   2, 217); lv_obj_set_size(ui_BtnEnBase,  148, 31);
    lv_obj_set_style_bg_color(ui_BtnEnBase,  lv_color_make(0x20,0x20,0x2E), 0);
    lv_obj_set_style_border_color(ui_BtnEnBase, lv_color_make(0x48,0x48,0x60), 0);
    lv_obj_set_style_border_width(ui_BtnEnBase,  2, 0);
    lv_obj_set_style_radius(ui_BtnEnBase, 8, 0);
    lv_obj_set_style_shadow_width(ui_BtnEnBase, 0, 0);
    lv_obj_add_event_cb(ui_BtnEnBase,  cb_toggle_base,  LV_EVENT_CLICKED, NULL);
    ui_LabelBtnBase = lv_label_create(ui_BtnEnBase);
    lv_label_set_text(ui_LabelBtnBase, "BASE OFF");
    lv_obj_set_style_text_font(ui_LabelBtnBase, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelBtnBase, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelBtnBase, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_center(ui_LabelBtnBase);

    ui_BtnEnCielo = lv_btn_create(ui_ScreenMain);
    lv_obj_set_pos(ui_BtnEnCielo, 154, 217); lv_obj_set_size(ui_BtnEnCielo, 148, 31);
    lv_obj_set_style_bg_color(ui_BtnEnCielo, lv_color_make(0x20,0x20,0x2E), 0);
    lv_obj_set_style_border_color(ui_BtnEnCielo, lv_color_make(0x48,0x48,0x60), 0);
    lv_obj_set_style_border_width(ui_BtnEnCielo, 2, 0);
    lv_obj_set_style_radius(ui_BtnEnCielo, 8, 0);
    lv_obj_set_style_shadow_width(ui_BtnEnCielo, 0, 0);
    lv_obj_add_event_cb(ui_BtnEnCielo, cb_toggle_cielo, LV_EVENT_CLICKED, NULL);
    ui_LabelBtnCielo = lv_label_create(ui_BtnEnCielo);
    lv_label_set_text(ui_LabelBtnCielo, "CIELO OFF");
    lv_obj_set_style_text_font(ui_LabelBtnCielo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelBtnCielo, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelBtnCielo, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_center(ui_LabelBtnCielo);

    ui_BtnLuce = lv_btn_create(ui_ScreenMain);
    lv_obj_set_pos(ui_BtnLuce, 306, 217); lv_obj_set_size(ui_BtnLuce, 90, 31);
    lv_obj_set_style_bg_color(ui_BtnLuce, lv_color_make(0x20,0x20,0x2E), 0);
    lv_obj_set_style_border_color(ui_BtnLuce, lv_color_make(0x48,0x48,0x60), 0);
    lv_obj_set_style_border_width(ui_BtnLuce, 2, 0);
    lv_obj_set_style_radius(ui_BtnLuce, 8, 0);
    lv_obj_set_style_shadow_width(ui_BtnLuce, 0, 0);
    lv_obj_add_event_cb(ui_BtnLuce, cb_toggle_luce, LV_EVENT_CLICKED, NULL);
    ui_LabelBtnLuce = lv_label_create(ui_BtnLuce);
    lv_label_set_text(ui_LabelBtnLuce, LV_SYMBOL_CHARGE " LUCE");
    lv_obj_set_style_text_font(ui_LabelBtnLuce, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelBtnLuce, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelBtnLuce, lv_color_make(0xA0,0xA0,0xB8),
                                LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_center(ui_LabelBtnLuce);

    // Sep y=249
    make_sep(ui_ScreenMain, 249, UI_COL_DARKGRAY);

    // Status bar y=251 h=21
    lv_obj_t* sb = lv_obj_create(ui_ScreenMain);
    lv_obj_set_pos(sb, 0, 251); lv_obj_set_size(sb, SCR_W, 21);
    lv_obj_set_style_bg_color(sb, lv_color_make(0x0A,0x0A,0x14), 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);

    ui_LedFan = lv_led_create(sb);
    lv_obj_set_pos(ui_LedFan, SCR_W - 24, 1); lv_obj_set_size(ui_LedFan, 16, 16);
    lv_led_set_color(ui_LedFan, UI_COL_CYAN); lv_led_off(ui_LedFan);
    lv_obj_t* lfan = lv_label_create(sb); lv_label_set_text(lfan, "FAN");
    lv_obj_set_style_text_font(lfan, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lfan, lv_color_make(0x40,0x40,0x55), 0);
    lv_obj_align(lfan, LV_ALIGN_RIGHT_MID, -4, 0);

    ui_LabelStatus = lv_label_create(sb);
    lv_label_set_text(ui_LabelStatus, "Sistema pronto");
    lv_obj_set_style_text_font(ui_LabelStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_GRAY, 0);
    lv_obj_align(ui_LabelStatus, LV_ALIGN_LEFT_MID, 6, 0);

    // Safety banner
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

    // ── ANIM 14: barre preheat — sottili (h=4) sopra il separatore
    //    Pannello BASE:  x=2   y=211  w=234
    //    Pannello CIELO: x=244 y=211  w=234
    auto make_preheat_bar = [](lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w) -> lv_obj_t* {
        lv_obj_t* b = lv_bar_create(parent);
        lv_obj_set_pos(b, x, y);
        lv_obj_set_size(b, w, 4);
        lv_bar_set_range(b, 0, 100);
        lv_bar_set_value(b, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(b, lv_color_make(0x18,0x18,0x28), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, lv_color_make(0x00,0x80,0xFF), LV_PART_INDICATOR);
        lv_obj_set_style_radius(b, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(b, 2, LV_PART_INDICATOR);
        lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);   // nascosta finché non inizia preheat
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        return b;
    };
    ui_BarPreheatBase  = make_preheat_bar(ui_ScreenMain, 2,   211, 234);
    ui_BarPreheatCielo = make_preheat_bar(ui_ScreenMain, 244, 211, 234);
}

// ================================================================
//  BUILD TEMP/CTRL
// ================================================================
static void build_temp() {
    ui_ScreenTemp = make_screen(UI_COL_BG);

    ui_TempBtnEnBase  = make_ctrl(ui_ScreenTemp,   1, 1, 115, 30,
        "BASE OFF", cb_toggle_base, &ui_TempLblBtnBase);
    ui_TempBtnEnCielo = make_ctrl(ui_ScreenTemp, 118, 1, 115, 30,
        "CIELO OFF", cb_toggle_cielo, &ui_TempLblBtnCielo);
    ui_TempBtnLuce    = make_ctrl(ui_ScreenTemp, 235, 1, 78, 30,
        LV_SYMBOL_CHARGE " LUCE", cb_toggle_luce, &ui_TempLblBtnLuce);

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

    // Panel BASE
    lv_obj_t* pb = lv_obj_create(ui_ScreenTemp);
    lv_obj_set_pos(pb, 2, 33); lv_obj_set_size(pb, 234, 150);
    lv_obj_set_style_bg_color(pb, lv_color_make(0x14,0x14,0x22), 0);
    lv_obj_set_style_bg_opa(pb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pb, UI_COL_ACCENT, 0);
    lv_obj_set_style_border_width(pb, 2, 0);
    lv_obj_set_style_radius(pb, 10, 0);
    lv_obj_set_style_pad_all(pb, 6, 0);
    lv_obj_clear_flag(pb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lb = lv_label_create(pb); lv_label_set_text(lb, "BASE");
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lb, UI_COL_ACCENT, 0); lv_obj_set_pos(lb, 0, 0);

    ui_ArcBase = make_arc(pb, 0, 14, 112, UI_COL_ACCENT);
    ui_TempSetBase = lv_label_create(pb);
    lv_label_set_text(ui_TempSetBase, "250\xC2\xB0""C");
    lv_obj_set_style_text_font(ui_TempSetBase, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ui_TempSetBase, UI_COL_YELLOW, 0);
    lv_obj_set_pos(ui_TempSetBase, 0, 56); lv_obj_set_size(ui_TempSetBase, 112, 28);
    lv_obj_set_style_text_align(ui_TempSetBase, LV_TEXT_ALIGN_CENTER, 0);

    // Bottoni ± BASE con bounce (gestito da make_pm)
    make_pm(pb, 114, 14, 108, 52, "+", UI_COL_ACCENT, cb_base_plus);
    make_pm(pb, 114, 70, 108, 52, "-", UI_COL_ACCENT, cb_base_minus);

    lv_obj_t* sbl = lv_label_create(pb);
    lv_label_set_text(sbl, "\xC2\xB1""5\xC2\xB0""C");
    lv_obj_set_style_text_font(sbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(sbl, UI_COL_GRAY, 0);
    lv_obj_set_pos(sbl, 144, 126);

    ui_TempErrBase = lv_label_create(pb); lv_label_set_text(ui_TempErrBase, "");
    lv_obj_set_style_text_font(ui_TempErrBase, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_TempErrBase, lv_color_make(0xFF,0x40,0x40), 0);
    lv_obj_set_pos(ui_TempErrBase, 0, 126);

    ui_TempBarBase = make_bar(pb, 0, 138, 222, 8, UI_COL_ACCENT);

    // Panel CIELO
    lv_obj_t* pc = lv_obj_create(ui_ScreenTemp);
    lv_obj_set_pos(pc, 238, 33); lv_obj_set_size(pc, 240, 150);
    lv_obj_set_style_bg_color(pc, lv_color_make(0x14,0x14,0x22), 0);
    lv_obj_set_style_bg_opa(pc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pc, UI_COL_CIELO, 0);
    lv_obj_set_style_border_width(pc, 2, 0);
    lv_obj_set_style_radius(pc, 10, 0);
    lv_obj_set_style_pad_all(pc, 6, 0);
    lv_obj_clear_flag(pc, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lc = lv_label_create(pc); lv_label_set_text(lc, "CIELO");
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lc, UI_COL_CIELO, 0); lv_obj_set_pos(lc, 0, 0);

    ui_ArcCielo = make_arc(pc, 0, 14, 112, UI_COL_CIELO);
    ui_TempSetCielo = lv_label_create(pc);
    lv_label_set_text(ui_TempSetCielo, "300\xC2\xB0""C");
    lv_obj_set_style_text_font(ui_TempSetCielo, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ui_TempSetCielo, UI_COL_YELLOW, 0);
    lv_obj_set_pos(ui_TempSetCielo, 0, 56); lv_obj_set_size(ui_TempSetCielo, 112, 28);
    lv_obj_set_style_text_align(ui_TempSetCielo, LV_TEXT_ALIGN_CENTER, 0);

    // Bottoni ± CIELO con bounce (gestito da make_pm)
    make_pm(pc, 114, 14, 120, 52, "+", UI_COL_CIELO, cb_cielo_plus);
    make_pm(pc, 114, 70, 120, 52, "-", UI_COL_CIELO, cb_cielo_minus);

    lv_obj_t* scl = lv_label_create(pc);
    lv_label_set_text(scl, "\xC2\xB1""5\xC2\xB0""C");
    lv_obj_set_style_text_font(scl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(scl, UI_COL_GRAY, 0);
    lv_obj_set_pos(scl, 148, 126);

    ui_TempErrCielo = lv_label_create(pc); lv_label_set_text(ui_TempErrCielo, "");
    lv_obj_set_style_text_font(ui_TempErrCielo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_TempErrCielo, lv_color_make(0xFF,0x40,0x40), 0);
    lv_obj_set_pos(ui_TempErrCielo, 0, 126);

    ui_TempBarCielo = make_bar(pc, 0, 138, 228, 8, UI_COL_CIELO);

    // Sep y=183
    make_sep(ui_ScreenTemp, 183, lv_color_make(0x30,0x30,0x48));

    // Nav azioni y=185 h=40
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
        // ← ANIMAZIONE 5: bounce
        lv_obj_add_event_cb(b, cb_anim_bounce, LV_EVENT_PRESSED, NULL);
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

    // Sep y=225
    make_sep(ui_ScreenTemp, 225, UI_COL_SINGLE);

    // Mode toggle
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

    // PanelSplit
    ui_PanelSplit = lv_obj_create(ui_ScreenTemp);
    lv_obj_set_pos(ui_PanelSplit, 154, 227); lv_obj_set_size(ui_PanelSplit, 324, 43);
    lv_obj_set_style_bg_color(ui_PanelSplit, lv_color_make(0x12,0x00,0x22), 0);
    lv_obj_set_style_border_color(ui_PanelSplit, UI_COL_SINGLE, 0);
    lv_obj_set_style_border_width(ui_PanelSplit, 1, 0);
    lv_obj_set_style_radius(ui_PanelSplit, 8, 0);
    lv_obj_set_style_pad_all(ui_PanelSplit, 4, 0);
    lv_obj_clear_flag(ui_PanelSplit, LV_OBJ_FLAG_SCROLLABLE);

    ui_LabelPctBase = lv_label_create(ui_PanelSplit);
    lv_label_set_text(ui_LabelPctBase, "B 100%");
    lv_obj_set_style_text_font(ui_LabelPctBase, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelPctBase, UI_COL_ACCENT, 0);
    lv_obj_set_pos(ui_LabelPctBase, 2, 12);

    // Bottoni PanelSplit con bounce (gestito da make_pm via lv_obj_add_event_cb diretto)
    ui_BtnPctBaseMinus = make_pm(ui_PanelSplit,  66, 4, 40, 35, "-", UI_COL_ACCENT, cb_pct_base_minus);
    ui_BtnPctBasePlus  = make_pm(ui_PanelSplit, 110, 4, 40, 35, "+", UI_COL_ACCENT, cb_pct_base_plus);

    lv_obj_t* vsep = lv_obj_create(ui_PanelSplit);
    lv_obj_set_pos(vsep, 154, 4); lv_obj_set_size(vsep, 2, 35);
    lv_obj_set_style_bg_color(vsep, UI_COL_SINGLE, 0);
    lv_obj_set_style_bg_opa(vsep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vsep, 0, 0);

    ui_LabelPctCielo = lv_label_create(ui_PanelSplit);
    lv_label_set_text(ui_LabelPctCielo, "C 100%");
    lv_obj_set_style_text_font(ui_LabelPctCielo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelPctCielo, UI_COL_CIELO, 0);
    lv_obj_set_pos(ui_LabelPctCielo, 160, 12);

    ui_BtnPctCieloMinus = make_pm(ui_PanelSplit, 224, 4, 40, 35, "-", UI_COL_CIELO, cb_pct_cielo_minus);
    ui_BtnPctCieloPlus  = make_pm(ui_PanelSplit, 268, 4, 40, 35, "+", UI_COL_CIELO, cb_pct_cielo_plus);

    lv_obj_add_flag(ui_PanelSplit, LV_OBJ_FLAG_HIDDEN);

    ui_TempModeLbl = lv_label_create(ui_ScreenTemp); lv_label_set_text(ui_TempModeLbl, "");
    lv_obj_add_flag(ui_TempModeLbl, LV_OBJ_FLAG_HIDDEN);
    ui_TempStatusLbl = lv_label_create(ui_ScreenTemp); lv_label_set_text(ui_TempStatusLbl, "");
    lv_obj_add_flag(ui_TempStatusLbl, LV_OBJ_FLAG_HIDDEN);
    ui_TempFanLbl = lv_label_create(ui_ScreenTemp); lv_label_set_text(ui_TempFanLbl, "");
    lv_obj_add_flag(ui_TempFanLbl, LV_OBJ_FLAG_HIDDEN);
    ui_TempSafetyLbl = lv_label_create(ui_ScreenTemp); lv_label_set_text(ui_TempSafetyLbl, "");
    lv_obj_add_flag(ui_TempSafetyLbl, LV_OBJ_FLAG_HIDDEN);
    ui_LabelSplitCielo = lv_label_create(ui_ScreenTemp); lv_label_set_text(ui_LabelSplitCielo, "");
    lv_obj_add_flag(ui_LabelSplitCielo, LV_OBJ_FLAG_HIDDEN);
    ui_BtnSplitMinus = lv_btn_create(ui_ScreenTemp); lv_obj_add_flag(ui_BtnSplitMinus, LV_OBJ_FLAG_HIDDEN);
    ui_BtnSplitPlus  = lv_btn_create(ui_ScreenTemp); lv_obj_add_flag(ui_BtnSplitPlus,  LV_OBJ_FLAG_HIDDEN);
}

// ================================================================
//  BUILD PID (comune per BASE e CIELO)
// ================================================================
static void build_pid_screen(lv_obj_t** scr_out, lv_color_t hcol, const char* htxt,
    const char* btn_l_txt, lv_color_t btn_l_col, lv_event_cb_t btn_l_cb,
    const char* btn_r_txt, lv_color_t btn_r_col, lv_event_cb_t btn_r_cb,
    lv_event_cb_t cb_kp_m, lv_event_cb_t cb_kp_p, lv_obj_t** lbl_kp,
    lv_event_cb_t cb_ki_m, lv_event_cb_t cb_ki_p, lv_obj_t** lbl_ki,
    lv_event_cb_t cb_kd_m, lv_event_cb_t cb_kd_p, lv_obj_t** lbl_kd,
    lv_obj_t** lbl_set, lv_obj_t** lbl_save) {

    *scr_out = make_screen(UI_COL_BG);
    lv_obj_t* scr = *scr_out;
    make_header(scr, hcol, htxt);

    auto make_pid_btn = [&](int x, lv_color_t bg, lv_color_t border,
                            const char* txt, lv_color_t tcol, lv_event_cb_t cb) {
        lv_obj_t* b = lv_btn_create(scr);
        lv_obj_set_pos(b, x, 2); lv_obj_set_size(b, 156, 28);
        lv_obj_set_style_bg_color(b, bg, 0);
        lv_obj_set_style_border_color(b, border, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, tcol, 0); lv_obj_center(l);
    };
    make_pid_btn(  2, lv_color_make(0x0A,0x0A,0x14), btn_l_col, btn_l_txt, btn_l_col, btn_l_cb);
    make_pid_btn(322, lv_color_make(0x0A,0x0A,0x14), btn_r_col, btn_r_txt, btn_r_col, btn_r_cb);

    auto make_pid_row = [&](int y, const char* name, lv_color_t col,
                            lv_event_cb_t cm, lv_event_cb_t cp, lv_obj_t** lbl) {
        lv_obj_t* panel = lv_obj_create(scr);
        lv_obj_set_pos(panel, 2, y); lv_obj_set_size(panel, 476, 72);
        lv_obj_set_style_bg_color(panel, lv_color_make(0x0E,0x0E,0x1E), 0);
        lv_obj_set_style_border_color(panel, col, 0);
        lv_obj_set_style_border_width(panel, 2, 0);
        lv_obj_set_style_radius(panel, 8, 0);
        lv_obj_set_style_pad_all(panel, 6, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ln = lv_label_create(panel); lv_label_set_text(ln, name);
        lv_obj_set_style_text_font(ln, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ln, col, 0); lv_obj_set_pos(ln, 2, 2);

        *lbl = lv_label_create(panel); lv_label_set_text(*lbl, "0.00");
        lv_obj_set_style_text_font(*lbl, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(*lbl, UI_COL_WHITE, 0);
        lv_obj_set_pos(*lbl, 100, 0);

        // Bounce sui bottoni PID
        lv_obj_t* bm = make_pm(panel, 300, 4, 80, 55, "-", col, cm);
        lv_obj_t* bp = make_pm(panel, 386, 4, 80, 55, "+", col, cp);
        (void)bm; (void)bp;
    };

    make_pid_row( 34, "Kp", hcol, cb_kp_m, cb_kp_p, lbl_kp);
    make_pid_row(110, "Ki", hcol, cb_ki_m, cb_ki_p, lbl_ki);
    make_pid_row(186, "Kd", hcol, cb_kd_m, cb_kd_p, lbl_kd);

    *lbl_set = lv_label_create(scr); lv_label_set_text(*lbl_set, "Set: ---\xC2\xB0""C");
    lv_obj_set_style_text_font(*lbl_set, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*lbl_set, UI_COL_GRAY, 0);
    lv_obj_set_pos(*lbl_set, 2, 260);

    lv_obj_t* bsave = lv_btn_create(scr);
    lv_obj_set_pos(bsave, 360, 255); lv_obj_set_size(bsave, 118, 30);
    lv_obj_set_style_bg_color(bsave, lv_color_make(0x00,0x28,0x14), 0);
    lv_obj_set_style_border_color(bsave, UI_COL_GREEN, 0);
    lv_obj_set_style_border_width(bsave, 2, 0);
    lv_obj_set_style_radius(bsave, 8, 0);
    lv_obj_set_style_shadow_width(bsave, 0, 0);
    lv_obj_add_event_cb(bsave, cb_pid_save, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(bsave, cb_anim_bounce, LV_EVENT_PRESSED, NULL);
    lv_obj_t* lsave = lv_label_create(bsave);
    lv_label_set_text(lsave, LV_SYMBOL_SAVE " SALVA");
    lv_obj_set_style_text_font(lsave, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lsave, UI_COL_GREEN, 0);
    lv_obj_center(lsave);

    *lbl_save = lv_label_create(scr); lv_label_set_text(*lbl_save, "");
    lv_obj_set_style_text_font(*lbl_save, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(*lbl_save, UI_COL_GREEN, 0);
    lv_obj_set_pos(*lbl_save, 160, 260);
}

static void build_pid_base() {
    build_pid_screen(&ui_ScreenPidBase,
        UI_COL_ACCENT, LV_SYMBOL_SETTINGS "  PID — BASE",
        "CTRL " LV_SYMBOL_RIGHT, lv_color_make(0x00,0x60,0x30), cb_goto_temp,
        LV_SYMBOL_LEFT " CIELO", UI_COL_CIELO,                  cb_goto_pid_cielo,
        cb_kp_base_m, cb_kp_base_p, &ui_LblKpBase,
        cb_ki_base_m, cb_ki_base_p, &ui_LblKiBase,
        cb_kd_base_m, cb_kd_base_p, &ui_LblKdBase,
        &ui_LblSetBaseTun, &ui_LblSaveStatus);

    // ── ANIM 19: barra autotune indeterminate — aggiunta su ui_ScreenPidBase
    // (La stessa variabile ui_BarAutotune viene usata per entrambe le schermate
    //  PID, poiché le schermate non sono visibili contemporaneamente)
    ui_BarAutotune = lv_bar_create(ui_ScreenPidBase);
    lv_obj_set_pos(ui_BarAutotune, 10, 200);
    lv_obj_set_size(ui_BarAutotune, 460, 8);
    lv_bar_set_range(ui_BarAutotune, 0, 100);
    lv_bar_set_value(ui_BarAutotune, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_BarAutotune, lv_color_make(0x18,0x18,0x28), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_BarAutotune, lv_color_make(0x00,0xC0,0xFF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui_BarAutotune, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_BarAutotune, 4, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(ui_BarAutotune, 0, LV_PART_MAIN);
    lv_obj_add_flag(ui_BarAutotune, LV_OBJ_FLAG_HIDDEN);  // visibile solo durante autotune
    lv_obj_clear_flag(ui_BarAutotune, LV_OBJ_FLAG_SCROLLABLE);
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
    // Nota: ui_BarAutotune è condivisa; su PID_CIELO non serve ridefinirla.
    // Se necessario in futuro, creare ui_BarAutotuneCielo separata.
}

// ================================================================
//  BUILD GRAPH
// ================================================================
static void build_graph() {
    #define GLIME lv_color_make(0x80,0xFF,0x40)
    #define GBG   lv_color_make(0x05,0x09,0x05)
    ui_ScreenGraph = make_screen(UI_COL_BG);

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
    lv_obj_t* leg_b = lv_label_create(hdr); lv_label_set_text(leg_b, "— BASE");
    lv_obj_set_style_text_font(leg_b, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(leg_b, lv_color_make(0xFF,0x6B,0x00), 0);
    lv_obj_align(leg_b, LV_ALIGN_RIGHT_MID, -116, 0);
    lv_obj_t* leg_c = lv_label_create(hdr); lv_label_set_text(leg_c, "— CIELO");
    lv_obj_set_style_text_font(leg_c, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(leg_c, lv_color_make(0xFF,0x30,0x30), 0);
    lv_obj_align(leg_c, LV_ALIGN_RIGHT_MID, -2, 0);

    lv_obj_t* bbk = lv_btn_create(ui_ScreenGraph);
    lv_obj_set_pos(bbk, 208, 2); lv_obj_set_size(bbk, 64, 28);
    lv_obj_set_style_bg_color(bbk, lv_color_make(0x30,0x60,0x10), 0);
    lv_obj_set_style_bg_opa(bbk, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bbk, 0, 0);
    lv_obj_set_style_radius(bbk, 6, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, cb_goto_main_from_graph, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbk = lv_label_create(bbk); lv_label_set_text(lbk, LV_SYMBOL_LEFT " OK");
    lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbk, GLIME, 0); lv_obj_center(lbk);

    // Scale Y labels
    for (int i = 0; i <= 4; i++) {
        int temp = i * 100;
        int y = 224 - i * 48;
        lv_obj_t* l = lv_label_create(ui_ScreenGraph);
        char buf[8]; snprintf(buf, sizeof(buf), "%d", temp);
        lv_label_set_text(l, buf);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(l, UI_COL_GRAY, 0);
        lv_obj_set_pos(l, 2, y);
    }

    ui_Chart = lv_chart_create(ui_ScreenGraph);
    lv_obj_set_pos(ui_Chart, 32, 32); lv_obj_set_size(ui_Chart, 446, 192);
    lv_obj_set_style_bg_color(ui_Chart, GBG, 0);
    lv_obj_set_style_bg_opa(ui_Chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ui_Chart, GLIME, 0);
    lv_obj_set_style_border_width(ui_Chart, 1, 0);
    lv_chart_set_type(ui_Chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_Chart, 180);
    lv_chart_set_range(ui_Chart, LV_CHART_AXIS_PRIMARY_Y, 0, 500);
    lv_obj_set_style_size(ui_Chart, 0, LV_PART_INDICATOR);
    lv_chart_set_div_line_count(ui_Chart, 5, 0);
    lv_obj_set_style_line_color(ui_Chart, lv_color_make(0x20,0x30,0x20), LV_PART_MAIN);

    ui_SerBase  = lv_chart_add_series(ui_Chart, UI_COL_ACCENT,  LV_CHART_AXIS_PRIMARY_Y);
    ui_SerCielo = lv_chart_add_series(ui_Chart, UI_COL_CIELO,   LV_CHART_AXIS_PRIMARY_Y);

    ui_GraphTimeLbl = lv_label_create(ui_ScreenGraph);
    lv_label_set_text(ui_GraphTimeLbl, "Ultimi 15 min");
    lv_obj_set_style_text_font(ui_GraphTimeLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_GraphTimeLbl, UI_COL_GRAY, 0);
    lv_obj_set_pos(ui_GraphTimeLbl, 32, 226);

    ui_GraphMaxLbl = lv_label_create(ui_ScreenGraph);
    lv_label_set_text(ui_GraphMaxLbl, ""); lv_obj_set_pos(ui_GraphMaxLbl, 300, 226);
    lv_obj_set_style_text_font(ui_GraphMaxLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_GraphMaxLbl, UI_COL_GRAY, 0);

    ui_GraphMinLbl = lv_label_create(ui_ScreenGraph);
    lv_label_set_text(ui_GraphMinLbl, ""); lv_obj_set_pos(ui_GraphMinLbl, 380, 226);
    lv_obj_set_style_text_font(ui_GraphMinLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_GraphMinLbl, UI_COL_GRAY, 0);

    make_sep(ui_ScreenGraph, 225, lv_color_make(0x30,0x60,0x10));

    auto make_preset = [](lv_obj_t* scr, int x, const char* txt,
                          lv_event_cb_t cb, bool active) -> lv_obj_t* {
        lv_obj_t* b = lv_btn_create(scr);
        lv_obj_set_pos(b, x, 228); lv_obj_set_size(b, 156, 40);
        lv_obj_set_style_bg_color(b, active ? GLIME : lv_color_make(0x0C,0x1C,0x0C), 0);
        lv_obj_set_style_border_color(b, GLIME, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(b, cb_anim_bounce, LV_EVENT_PRESSED, NULL);
        lv_obj_t* l = lv_label_create(b); lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, active ? lv_color_black() : GLIME, 0);
        lv_obj_center(l);
        return b;
    };
    ui_BtnPreset5  = make_preset(ui_ScreenGraph,   2, "Ultimi 5 min",  cb_preset_5,  false);
    ui_BtnPreset15 = make_preset(ui_ScreenGraph, 162, "Ultimi 15 min", cb_preset_15, true);
    ui_BtnPreset30 = make_preset(ui_ScreenGraph, 322, "Ultimi 30 min", cb_preset_30, false);
}

// ================================================================
//  BUILD TIMER
// ================================================================
static void build_timer() {
    ui_ScreenTimer = make_screen(UI_COL_BG);
    make_header(ui_ScreenTimer, lv_color_make(0x80,0x80,0xFF), LV_SYMBOL_LOOP "  TIMER COTTURA");

    lv_obj_t* bbk = lv_btn_create(ui_ScreenTimer);
    lv_obj_set_pos(bbk, 414, 2); lv_obj_set_size(bbk, 64, 28);
    lv_obj_set_style_bg_color(bbk, lv_color_make(0x10,0x10,0x28), 0);
    lv_obj_set_style_border_width(bbk, 0, 0);
    lv_obj_set_style_radius(bbk, 6, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, cb_goto_temp, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbk = lv_label_create(bbk); lv_label_set_text(lbk, LV_SYMBOL_LEFT " OK");
    lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbk, lv_color_make(0x80,0x80,0xFF), 0); lv_obj_center(lbk);

    ui_LabelTimerValue = lv_label_create(ui_ScreenTimer);
    lv_label_set_text(ui_LabelTimerValue, "10:00");
    lv_obj_set_style_text_font(ui_LabelTimerValue, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui_LabelTimerValue, lv_color_make(0x80,0x80,0xFF), 0);
    lv_obj_set_width(ui_LabelTimerValue, SCR_W);
    lv_obj_set_style_text_align(ui_LabelTimerValue, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(ui_LabelTimerValue, 0, 50);

    make_pm(ui_ScreenTimer,  20, 120, 200, 60, "-1", lv_color_make(0x80,0x80,0xFF), cb_timer_minus);
    make_pm(ui_ScreenTimer, 260, 120, 200, 60, "+1", lv_color_make(0x80,0x80,0xFF), cb_timer_plus);
    make_pm(ui_ScreenTimer,  20, 190, 200, 50, "-15", lv_color_make(0x50,0x50,0xAA), cb_timer_minus15);
    make_pm(ui_ScreenTimer, 260, 190, 200, 50, "+15", lv_color_make(0x50,0x50,0xAA), cb_timer_plus15);

    lv_obj_t* breset = lv_btn_create(ui_ScreenTimer);
    lv_obj_set_pos(breset, 140, 248); lv_obj_set_size(breset, 200, 22);
    lv_obj_set_style_bg_color(breset, lv_color_make(0x30,0x00,0x00), 0);
    lv_obj_set_style_border_color(breset, lv_color_make(0xFF,0x40,0x40), 0);
    lv_obj_set_style_border_width(breset, 1, 0);
    lv_obj_set_style_radius(breset, 6, 0);
    lv_obj_set_style_shadow_width(breset, 0, 0);
    lv_obj_add_event_cb(breset, cb_timer_reset, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lreset = lv_label_create(breset); lv_label_set_text(lreset, LV_SYMBOL_REFRESH " RESET");
    lv_obj_set_style_text_font(lreset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lreset, lv_color_make(0xFF,0x40,0x40), 0);
    lv_obj_center(lreset);

    ui_LabelTimerStatus = lv_label_create(ui_ScreenTimer);
    lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_CHARGE " Parte con resistenza ON");
    lv_obj_set_style_text_font(ui_LabelTimerStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui_LabelTimerStatus, UI_COL_GRAY, 0);
    lv_obj_set_width(ui_LabelTimerStatus, SCR_W);
    lv_obj_set_style_text_align(ui_LabelTimerStatus, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(ui_LabelTimerStatus, 0, 259);
}

// ================================================================
//  BUILD RICETTE
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

    lv_obj_t* bbk = lv_btn_create(ui_ScreenRicette);
    lv_obj_set_pos(bbk, 414, 2); lv_obj_set_size(bbk, 64, 28);
    lv_obj_set_style_bg_color(bbk, lv_color_make(0x00,0x28,0x14), 0);
    lv_obj_set_style_border_width(bbk, 0, 0);
    lv_obj_set_style_radius(bbk, 6, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, cb_goto_temp, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbk = lv_label_create(bbk); lv_label_set_text(lbk, LV_SYMBOL_LEFT " OK");
    lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbk, RICETTE_COL, 0); lv_obj_center(lbk);

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
        // ← ANIMAZIONE 12: feedback card al tocco
        lv_obj_add_event_cb(card, cb_anim_card_press, LV_EVENT_PRESSED, NULL);

        lv_obj_t* ln = lv_label_create(card); lv_label_set_text(ln, r.nome);
        lv_obj_set_style_text_font(ln, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ln, RICETTE_COL, 0);
        lv_obj_set_style_text_align(ln, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(ln, 218); lv_obj_set_pos(ln, 0, 2);

        char cbuf[20]; snprintf(cbuf, sizeof(cbuf), LV_SYMBOL_UP " %d\xC2\xB0""C", r.set_cielo);
        lv_obj_t* lc = lv_label_create(card); lv_label_set_text(lc, cbuf);
        lv_obj_set_style_text_font(lc, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lc, UI_COL_CIELO, 0);
        lv_obj_set_style_text_align(lc, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lc, 218); lv_obj_set_pos(lc, 0, 34);

        char bbuf[20]; snprintf(bbuf, sizeof(bbuf), LV_SYMBOL_DOWN " %d\xC2\xB0""C", r.set_base);
        lv_obj_t* lb2 = lv_label_create(card); lv_label_set_text(lb2, bbuf);
        lv_obj_set_style_text_font(lb2, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lb2, UI_COL_ACCENT, 0);
        lv_obj_set_style_text_align(lb2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lb2, 218); lv_obj_set_pos(lb2, 0, 64);
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
        // ← ANIMAZIONE 1+2: slide direzionale + fade-in su ogni schermata
        case Screen::MAIN:
            lv_scr_load_anim(ui_ScreenMain,     LV_SCR_LOAD_ANIM_MOVE_RIGHT,  220, 0, false);
            anim_fade_screen(ui_ScreenMain);     break;
        case Screen::TEMP:
            lv_scr_load_anim(ui_ScreenTemp,     LV_SCR_LOAD_ANIM_MOVE_LEFT,   220, 0, false);
            anim_fade_screen(ui_ScreenTemp);     break;
        case Screen::PID_BASE:
            lv_scr_load_anim(ui_ScreenPidBase,  LV_SCR_LOAD_ANIM_MOVE_TOP,    220, 0, false);
            anim_fade_screen(ui_ScreenPidBase);  break;
        case Screen::PID_CIELO:
            lv_scr_load_anim(ui_ScreenPidCielo, LV_SCR_LOAD_ANIM_MOVE_LEFT,   220, 0, false);
            anim_fade_screen(ui_ScreenPidCielo); break;
        case Screen::GRAPH:
            lv_scr_load_anim(ui_ScreenGraph,    LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 220, 0, false);
            anim_fade_screen(ui_ScreenGraph);
            anim_graph_intro(ui_Chart);          // ← ANIMAZIONE 18: chart slide-in
            break;
        case Screen::TIMER:
            lv_scr_load_anim(ui_ScreenTimer,    LV_SCR_LOAD_ANIM_MOVE_TOP,    220, 0, false);
            anim_fade_screen(ui_ScreenTimer);    break;
        case Screen::RICETTE:
            lv_scr_load_anim(ui_ScreenRicette,  LV_SCR_LOAD_ANIM_MOVE_TOP,    220, 0, false);
            anim_fade_screen(ui_ScreenRicette);  break;
        case Screen::WIFI_SCAN:
            ui_show_wifi_scan(); break;
        case Screen::WIFI_PWD:
            break;
        case Screen::OTA:
            ui_show_ota(); break;
    }
}

// ================================================================
//  ui_refresh — aggiorna MAIN
// ================================================================
void ui_refresh(AppState* s) {
    if (!s) return;

    // ← ANIMAZIONE 3: soglie per pulse temperatura (evita flash da jitter)
    static float s_last_base  = -9999.f;
    static float s_last_cielo = -9999.f;
    // ← ANIMAZIONE 4: traccia stato relay per glow (evita chiamate ridondanti)
    static bool s_relay_base_prev  = false;
    static bool s_relay_cielo_prev = false;

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
        // ← ANIMAZIONE 3: pulse se cambiato di almeno 1°
        if (fabsf(s->temp_base - s_last_base) > 0.9f) {
            anim_pulse_temp_label(ui_LabelTempBase);
            s_last_base = s->temp_base;
        }
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
        // ← ANIMAZIONE 3
        if (fabsf(s->temp_cielo - s_last_cielo) > 0.9f) {
            anim_pulse_temp_label(ui_LabelTempCielo);
            s_last_cielo = s->temp_cielo;
        }
    }

    // Barre PID animate
    {
        int v = (int)s->pid_out_base;
        anim_set_bar(ui_BarPidBase, v);   // ← ANIMAZIONE 8: LV_ANIM_ON
        char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
        lv_label_set_text(ui_LabelPidBase, buf);
    }
    {
        int v = (int)s->pid_out_cielo;
        anim_set_bar(ui_BarPidCielo, v);  // ← ANIMAZIONE 8
        char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
        lv_label_set_text(ui_LabelPidCielo, buf);
    }

    // LED relay
    if (s->relay_base)  lv_led_on(ui_LedRelayBase);  else lv_led_off(ui_LedRelayBase);
    if (s->relay_cielo) lv_led_on(ui_LedRelayCielo); else lv_led_off(ui_LedRelayCielo);

    // ← ANIMAZIONE 9: lampeggio LED all'attivazione
    if (s->relay_base  && !s_relay_base_prev)  anim_relay_led_on(ui_LedRelayBase);
    if (s->relay_cielo && !s_relay_cielo_prev) anim_relay_led_on(ui_LedRelayCielo);

    // ← ANIMAZIONE 4: glow pulsante ON attivo / stop glow quando OFF
    if (s->relay_base)  anim_glow_relay_btn(ui_BtnEnBase,  UI_COL_ACCENT);
    else                anim_stop_glow(ui_BtnEnBase);
    if (s->relay_cielo) anim_glow_relay_btn(ui_BtnEnCielo, UI_COL_CIELO);
    else                anim_stop_glow(ui_BtnEnCielo);

    s_relay_base_prev  = s->relay_base;
    s_relay_cielo_prev = s->relay_cielo;

    // Bottoni ON/OFF BASE
    if (s->base_enabled) {
        lv_obj_set_style_bg_color(ui_BtnEnBase, lv_color_make(0x28,0x10,0x00), 0);
        lv_obj_set_style_border_color(ui_BtnEnBase, UI_COL_ACCENT, 0);
        lv_label_set_text(ui_LabelBtnBase, s->preheat_base ?
            LV_SYMBOL_WARNING " PRERISCALDO" : "BASE ON");
        lv_obj_set_style_text_color(ui_LabelBtnBase, UI_COL_ACCENT,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnBase, UI_COL_ACCENT,
                                    LV_PART_MAIN | LV_STATE_PRESSED);
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
        lv_label_set_text(ui_LabelBtnCielo, s->preheat_cielo ?
            LV_SYMBOL_WARNING " PRERISCALDO" : "CIELO ON");
        lv_obj_set_style_text_color(ui_LabelBtnCielo, UI_COL_CIELO,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnCielo, UI_COL_CIELO,
                                    LV_PART_MAIN | LV_STATE_PRESSED);
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
        lv_obj_set_style_text_color(ui_LabelBtnLuce, UI_COL_LUCE,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelBtnLuce, UI_COL_LUCE,
                                    LV_PART_MAIN | LV_STATE_PRESSED);
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
        // ← ANIMAZIONE 11: banner flash urgente
        anim_safety_banner(ui_PanelSafety);
    } else {
        lv_obj_add_flag(ui_PanelSafety, LV_OBJ_FLAG_HIDDEN);
        anim_safety_banner_stop(ui_PanelSafety);
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
        snprintf(buf, sizeof(buf), "B:%.0f/%.0f\xC2\xB0  C:%.0f/%.0f\xC2\xB0",
            s->temp_base, s->set_base, s->temp_cielo, s->set_cielo);
        lv_label_set_text(ui_LabelStatus, buf);
        lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_GRAY, 0);
    }

    // ← ANIMAZIONE 15: fan LED breathing
    {
        static bool s_fan_prev = false;
        if (s->fan_on && !s_fan_prev) {
            lv_led_on(ui_LedFan);
            anim_fan_breathing_start(ui_LedFan);
        } else if (!s->fan_on && s_fan_prev) {
            anim_fan_breathing_stop(ui_LedFan);
            lv_led_off(ui_LedFan);
        }
        s_fan_prev = s->fan_on;
    }

    // ← ANIMAZIONE 14: barra preheat con colore termico
    if (s->preheat_base && !s->tc_base_err && s->set_base > 0)
        anim_preheat_bar(ui_BarPreheatBase,
                         (int)(s->temp_base / s->set_base * 100.0));
    else
        anim_preheat_bar_stop(ui_BarPreheatBase);

    if (s->preheat_cielo && !s->tc_cielo_err && s->set_cielo > 0)
        anim_preheat_bar(ui_BarPreheatCielo,
                         (int)(s->temp_cielo / s->set_cielo * 100.0));
    else
        anim_preheat_bar_stop(ui_BarPreheatCielo);
}

// ================================================================
//  ui_refresh_temp — aggiorna schermata CTRL
// ================================================================
void ui_refresh_temp(AppState* s) {
    if (!s) return;

    // ← ANIMAZIONE 10: archi setpoint con transizione animata
    anim_arc_set_value(ui_ArcBase,  (int)s->set_base);
    anim_arc_set_value(ui_ArcCielo, (int)s->set_cielo);

    char buf[20];
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0""C", s->set_base);
    lv_label_set_text(ui_TempSetBase, buf);
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0""C", s->set_cielo);
    lv_label_set_text(ui_TempSetCielo, buf);

    // Barre PID CTRL animate
    anim_set_bar(ui_TempBarBase,  (int)s->pid_out_base);
    anim_set_bar(ui_TempBarCielo, (int)s->pid_out_cielo);

    lv_label_set_text(ui_TempErrBase,  s->tc_base_err  ? "TC ERR" : "");
    lv_label_set_text(ui_TempErrCielo, s->tc_cielo_err ? "TC ERR" : "");

    // Bottoni ON/OFF CTRL
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
    if (s->luce_on) {
        lv_obj_set_style_border_color(ui_TempBtnLuce, UI_COL_LUCE, 0);
        lv_label_set_text(ui_TempLblBtnLuce, LV_SYMBOL_CHARGE " ON");
        lv_obj_set_style_text_color(ui_TempLblBtnLuce, UI_COL_LUCE, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_border_color(ui_TempBtnLuce, lv_color_make(0x60,0x60,0x80), 0);
        lv_label_set_text(ui_TempLblBtnLuce, LV_SYMBOL_CHARGE " LUCE");
        lv_obj_set_style_text_color(ui_TempLblBtnLuce, lv_color_make(0xA0,0xA0,0xB8),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Mode/split
    bool is_single = (s->sensor_mode == SensorMode::SINGLE);
    if (is_single) {
        lv_label_set_text(ui_LabelMode, LV_SYMBOL_PAUSE " SINGLE");
        lv_obj_set_style_text_color(ui_LabelMode, UI_COL_SINGLE, 0);
        lv_obj_set_style_border_color(ui_BtnModeToggle, UI_COL_SINGLE, 0);
        lv_obj_add_flag(ui_PanelSplit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(ui_LabelMode, LV_SYMBOL_SHUFFLE " DUAL");
        lv_obj_set_style_text_color(ui_LabelMode, UI_COL_DUAL, 0);
        lv_obj_set_style_border_color(ui_BtnModeToggle, UI_COL_DUAL, 0);
        lv_obj_clear_flag(ui_PanelSplit, LV_OBJ_FLAG_HIDDEN);
    }

    char pb[10], pc[10];
    snprintf(pb, sizeof(pb), "B %d%%", s->pct_base);
    snprintf(pc, sizeof(pc), "C %d%%", s->pct_cielo);
    lv_label_set_text(ui_LabelPctBase,  pb);
    lv_label_set_text(ui_LabelPctCielo, pc);
}

// ================================================================
//  ui_refresh_pid — aggiorna schermate PID
// ================================================================
void ui_refresh_pid(AppState* s) {
    if (!s) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", s->kp_base);  lv_label_set_text(ui_LblKpBase,  buf);
    snprintf(buf, sizeof(buf), "%.3f", s->ki_base);  lv_label_set_text(ui_LblKiBase,  buf);
    snprintf(buf, sizeof(buf), "%.2f", s->kd_base);  lv_label_set_text(ui_LblKdBase,  buf);
    snprintf(buf, sizeof(buf), "Set: %.0f\xC2\xB0""C", s->set_base);
    lv_label_set_text(ui_LblSetBaseTun, buf);

    snprintf(buf, sizeof(buf), "%.2f", s->kp_cielo); lv_label_set_text(ui_LblKpCielo, buf);
    snprintf(buf, sizeof(buf), "%.3f", s->ki_cielo); lv_label_set_text(ui_LblKiCielo, buf);
    snprintf(buf, sizeof(buf), "%.2f", s->kd_cielo); lv_label_set_text(ui_LblKdCielo, buf);
    snprintf(buf, sizeof(buf), "Set: %.0f\xC2\xB0""C", s->set_cielo);
    lv_label_set_text(ui_LblSetCieloTun, buf);

    // ← ANIMAZIONE 19: barra indeterminate durante autotune
    if (ui_BarAutotune) {
        if (s->autotune_status == AutotuneStatus::RUNNING) {
            anim_autotune_bar_start(ui_BarAutotune);
        } else {
            anim_autotune_bar_stop(ui_BarAutotune, s->autotune_cycles);
        }
    }
}

// ================================================================
//  ui_refresh_graph — aggiorna grafico
// ================================================================
void ui_refresh_graph(AppState* s) {
    if (!s || !ui_Chart) return;
    int samples = (g_graph_minutes * 60) / GRAPH_SAMPLE_S;
    if (samples > GRAPH_BUF_SIZE) samples = GRAPH_BUF_SIZE;
    if (g_graph.count == 0) return;

    lv_chart_set_point_count(ui_Chart, samples);
    lv_chart_set_all_value(ui_Chart, ui_SerBase,  LV_CHART_POINT_NONE);
    lv_chart_set_all_value(ui_Chart, ui_SerCielo, LV_CHART_POINT_NONE);

    float max_t = -1.f, min_t = 9999.f;
    int count = (g_graph.count < (uint16_t)samples) ? g_graph.count : samples;
    for (int i = 0; i < count; i++) {
        int idx = (g_graph.head - count + i + GRAPH_BUF_SIZE) % GRAPH_BUF_SIZE;
        float tb = g_graph.base[idx];
        float tc = g_graph.cielo[idx];
        lv_chart_set_next_value(ui_Chart, ui_SerBase,  (lv_coord_t)tb);
        lv_chart_set_next_value(ui_Chart, ui_SerCielo, (lv_coord_t)tc);
        if (tb > max_t) max_t = tb;
        if (tc > max_t) max_t = tc;
        if (tb < min_t) min_t = tb;
        if (tc < min_t) min_t = tc;
    }
    lv_chart_refresh(ui_Chart);

    char buf[24];
    if (max_t > 0) { snprintf(buf, sizeof(buf), "Max %.0f\xC2\xB0", max_t); lv_label_set_text(ui_GraphMaxLbl, buf); }
    if (min_t < 9999) { snprintf(buf, sizeof(buf), "Min %.0f\xC2\xB0", min_t); lv_label_set_text(ui_GraphMinLbl, buf); }
}

// ================================================================
//  ui_timer_auto_start / ui_timer_tick_1s
//  (implementazione minima — la logica reale è in ui_events.cpp)
// ================================================================
void ui_timer_auto_start(void) { /* gestito da ui_events */ }
void ui_timer_tick_1s(void)    { /* gestito da ui_events */ }

// ================================================================
//  ui_show_toast — wrapper pubblico per ANIM 16
//  Chiama da qualsiasi task (via lv_timer_create o direttamente
//  dal Task_LVGL) per mostrare una notifica overlay 2 secondi.
//  Esempio: ui_show_toast("MQTT: setpoint aggiornato");
// ================================================================
void ui_show_toast(const char* msg) {
    lv_obj_t* scr = lv_scr_act();
    if (!scr) return;
    anim_toast_show(scr, msg, &s_toast_ref);
}
