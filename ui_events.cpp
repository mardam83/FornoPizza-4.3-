/**
 * ui_events.cpp — Callback eventi v17
 * ================================================================
 * NOTA PORTING ESP32-S3:
 *   Le funzioni ui_refresh*, ui_timer_auto_start, ui_timer_tick_1s
 *   sono state spostate in ui.cpp per evitare doppie definizioni.
 *   Questo file contiene SOLO le callback cb_* e gli helper locali.
 * ================================================================
 */

#include "ui.h"
#include "hardware.h"
#include <Arduino.h>

// ================================================================
//  HELPER
// ================================================================
static void mark_dirty() { g_state.nvs_dirty = true; }

static void refresh_active() {
  mark_dirty();
  switch (g_state.active_screen) {
    case Screen::MAIN:      ui_refresh(&g_state);      break;
    case Screen::TEMP:      ui_refresh_temp(&g_state); break;
    case Screen::PID_BASE:
    case Screen::PID_CIELO: ui_refresh_pid(&g_state);  break;
    default: break;
  }
}

// ================================================================
//  CALLBACKS — setpoint
// ================================================================
void cb_base_minus(lv_event_t*) {
  g_state.set_base = fmax(50.0, g_state.set_base - 5.0);
  refresh_active();
}
void cb_base_plus(lv_event_t*) {
  g_state.set_base = fmin(500.0, g_state.set_base + 5.0);
  refresh_active();
}
void cb_cielo_minus(lv_event_t*) {
  g_state.set_cielo = fmax(50.0, g_state.set_cielo - 5.0);
  refresh_active();
}
void cb_cielo_plus(lv_event_t*) {
  g_state.set_cielo = fmin(500.0, g_state.set_cielo + 5.0);
  refresh_active();
}

// ================================================================
//  CALLBACKS — ON/OFF
// ================================================================
void cb_toggle_base(lv_event_t*) {
  if (g_state.sensor_mode == SensorMode::SINGLE) {
    bool ns = !g_state.base_enabled;
    g_state.base_enabled = g_state.cielo_enabled = ns;
  } else {
    g_state.base_enabled = !g_state.base_enabled;
  }
  ui_refresh(&g_state);
  ui_refresh_temp(&g_state);
}
void cb_toggle_cielo(lv_event_t*) {
  if (g_state.sensor_mode == SensorMode::SINGLE) {
    bool ns = !g_state.cielo_enabled;
    g_state.base_enabled = g_state.cielo_enabled = ns;
  } else {
    g_state.cielo_enabled = !g_state.cielo_enabled;
  }
  ui_refresh(&g_state);
  ui_refresh_temp(&g_state);
}
void cb_toggle_luce(lv_event_t*) {
  g_state.luce_on = !g_state.luce_on;
  ui_refresh(&g_state);
  ui_refresh_temp(&g_state);
}

// ================================================================
//  CALLBACKS — mode/split
// ================================================================
void cb_toggle_mode(lv_event_t*) {
  if (g_state.sensor_mode == SensorMode::SINGLE) {
    g_state.sensor_mode = SensorMode::DUAL;
  } else {
    g_state.sensor_mode = SensorMode::SINGLE;
    g_state.cielo_enabled = false;
  }
  g_state.nvs_dirty = true;
  refresh_active();
}

void cb_mode_long_press(lv_event_t*) {
  bool was_single = (g_state.sensor_mode == SensorMode::SINGLE);
  g_state.sensor_mode = was_single ? SensorMode::DUAL : SensorMode::SINGLE;
  if (g_state.sensor_mode == SensorMode::SINGLE)
    g_state.cielo_enabled = false;
  g_state.nvs_dirty = true;
  refresh_active();
}

void cb_pct_base_minus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.pct_base = max(0, g_state.pct_base - 5);
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
  refresh_active();
}
void cb_pct_base_plus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.pct_base = min(100, g_state.pct_base + 5);
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
  refresh_active();
}
void cb_pct_cielo_minus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.pct_cielo = max(0, g_state.pct_cielo - 5);
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
  refresh_active();
}
void cb_pct_cielo_plus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.pct_cielo = min(100, g_state.pct_cielo + 5);
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
  refresh_active();
}

// ================================================================
//  CALLBACKS — navigazione
// ================================================================
void cb_goto_temp(lv_event_t*)            { ui_show_screen(Screen::TEMP);      }
void cb_goto_main_from_temp(lv_event_t*)  { ui_show_screen(Screen::MAIN);      }
void cb_goto_pid_base(lv_event_t*)        { ui_show_screen(Screen::PID_BASE);  }
void cb_goto_pid_cielo(lv_event_t*)       { ui_show_screen(Screen::PID_CIELO); }
void cb_goto_graph(lv_event_t*)           { ui_show_screen(Screen::GRAPH);     }
void cb_goto_main_from_graph(lv_event_t*) { ui_show_screen(Screen::MAIN);      }
void cb_goto_timer(lv_event_t*)           { ui_show_screen(Screen::TIMER);     }
void cb_goto_ricette(lv_event_t*)         { ui_show_screen(Screen::RICETTE);   }
void cb_goto_wifi(lv_event_t*)            { ui_show_screen(Screen::WIFI_SCAN); }
void cb_goto_ota(lv_event_t*)             { ui_show_screen(Screen::OTA);       }

// ================================================================
//  CALLBACKS — grafico preset
// ================================================================
static void set_preset(int minutes) {
  g_graph_minutes = minutes;
  lv_color_t ca = lv_color_make(0x80,0xFF,0x40);
  lv_color_t ci = lv_color_make(0x0C,0x1C,0x0C);
  lv_obj_set_style_bg_color(ui_BtnPreset5,  minutes ==  5 ? ca : ci, 0);
  lv_obj_set_style_bg_color(ui_BtnPreset15, minutes == 15 ? ca : ci, 0);
  lv_obj_set_style_bg_color(ui_BtnPreset30, minutes == 30 ? ca : ci, 0);
  lv_color_t ta = lv_color_black();
  lv_color_t ti = lv_color_make(0x80,0xFF,0x40);
  lv_obj_t* btns[3] = {ui_BtnPreset5, ui_BtnPreset15, ui_BtnPreset30};
  int prs[3] = {5, 15, 30};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* l = lv_obj_get_child(btns[i], 0);
    if (l) lv_obj_set_style_text_color(l, minutes == prs[i] ? ta : ti, 0);
  }
  char buf[20];
  snprintf(buf, sizeof(buf), "Ultimi %d min", minutes);
  lv_label_set_text(ui_GraphTimeLbl, buf);
}
void cb_preset_5 (lv_event_t*) { set_preset(5);  }
void cb_preset_15(lv_event_t*) { set_preset(15); }
void cb_preset_30(lv_event_t*) { set_preset(30); }

// ================================================================
//  CALLBACKS — PID tuning
// ================================================================
void cb_kp_base_m(lv_event_t*)  { g_state.kp_base  = fmax(0.0,  g_state.kp_base  - 0.1);   refresh_active(); }
void cb_kp_base_p(lv_event_t*)  { g_state.kp_base  = fmin(20.0, g_state.kp_base  + 0.1);   refresh_active(); }
void cb_ki_base_m(lv_event_t*)  { g_state.ki_base  = fmax(0.0,  g_state.ki_base  - 0.005); refresh_active(); }
void cb_ki_base_p(lv_event_t*)  { g_state.ki_base  = fmin(1.0,  g_state.ki_base  + 0.005); refresh_active(); }
void cb_kd_base_m(lv_event_t*)  { g_state.kd_base  = fmax(0.0,  g_state.kd_base  - 0.1);   refresh_active(); }
void cb_kd_base_p(lv_event_t*)  { g_state.kd_base  = fmin(20.0, g_state.kd_base  + 0.1);   refresh_active(); }
void cb_kp_cielo_m(lv_event_t*) { g_state.kp_cielo = fmax(0.0,  g_state.kp_cielo - 0.1);   refresh_active(); }
void cb_kp_cielo_p(lv_event_t*) { g_state.kp_cielo = fmin(20.0, g_state.kp_cielo + 0.1);   refresh_active(); }
void cb_ki_cielo_m(lv_event_t*) { g_state.ki_cielo = fmax(0.0,  g_state.ki_cielo - 0.005); refresh_active(); }
void cb_ki_cielo_p(lv_event_t*) { g_state.ki_cielo = fmin(1.0,  g_state.ki_cielo + 0.005); refresh_active(); }
void cb_kd_cielo_m(lv_event_t*) { g_state.kd_cielo = fmax(0.0,  g_state.kd_cielo - 0.1);   refresh_active(); }
void cb_kd_cielo_p(lv_event_t*) { g_state.kd_cielo = fmin(20.0, g_state.kd_cielo + 0.1);   refresh_active(); }

void cb_pid_save(lv_event_t*) {
  g_state.nvs_dirty = true;
  lv_obj_t* lbl = (g_state.active_screen == Screen::PID_CIELO)
                   ? ui_LblSaveStatusCielo : ui_LblSaveStatus;
  lv_label_set_text(lbl, LV_SYMBOL_OK " Salvato!");
  lv_obj_set_style_text_color(lbl, UI_COL_GREEN, 0);
  lv_timer_t* t = lv_timer_create([](lv_timer_t* tmr) {
    lv_label_set_text(ui_LblSaveStatus,      "");
    lv_label_set_text(ui_LblSaveStatusCielo, "");
    lv_timer_del(tmr);
  }, 2000, nullptr);
}

// ================================================================
//  CALLBACKS — TIMER
// ================================================================
static int  s_timer_minutes = TIMER_DEFAULT_MIN;
static int  s_timer_seconds = 0;
static bool s_timer_running = false;

static void timer_refresh_display() {
  if (!ui_LabelTimerValue) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", s_timer_minutes, s_timer_seconds);
  lv_label_set_text(ui_LabelTimerValue, buf);
  if (ui_LabelTimerStatus)
    lv_label_set_text(ui_LabelTimerStatus,
      s_timer_running ? LV_SYMBOL_PLAY " IN CORSO" : "");
}

void cb_timer_plus(lv_event_t*) {
  if (!s_timer_running) {
    s_timer_minutes = min(s_timer_minutes + 5, 99);
    s_timer_seconds = 0;
    timer_refresh_display();
  }
}
void cb_timer_minus(lv_event_t*) {
  if (!s_timer_running) {
    s_timer_minutes = max(s_timer_minutes - 5, 1);
    s_timer_seconds = 0;
    timer_refresh_display();
  }
}
void cb_timer_plus15(lv_event_t*) {
  if (!s_timer_running) {
    s_timer_minutes = min(s_timer_minutes + 15, 99);
    s_timer_seconds = 0;
    timer_refresh_display();
  }
}
void cb_timer_minus15(lv_event_t*) {
  if (!s_timer_running) {
    s_timer_minutes = max(s_timer_minutes - 15, 1);
    s_timer_seconds = 0;
    timer_refresh_display();
  }
}
void cb_timer_reset(lv_event_t*) {
  s_timer_running = false;
  s_timer_minutes = TIMER_DEFAULT_MIN;
  s_timer_seconds = 0;
  timer_refresh_display();
  if (ui_LabelTimerStatus)
    lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_CHARGE " Parte con resistenza ON");
}

// ui_timer_auto_start e ui_timer_tick_1s sono in ui.cpp — NON ridefinire qui

// ================================================================
//  CALLBACKS — RICETTE
// ================================================================
struct RicettaExt { const char* nome; int set_base; int set_cielo; int timer_min; };
static const RicettaExt g_ricette_ev[] = {
  { "Margherita",  250, 300,  8 },
  { "Diavola",     260, 320,  9 },
  { "Calzone",     240, 280, 12 },
  { "Bianca",      255, 310,  8 },
};

void cb_ricetta_select(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx > 3) return;
  const RicettaExt& r = g_ricette_ev[idx];
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.set_base  = r.set_base;
    g_state.set_cielo = r.set_cielo;
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
  ui_show_screen(Screen::TEMP);
}