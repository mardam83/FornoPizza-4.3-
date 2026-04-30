/**
 * ui_events.cpp — Forno Pizza Controller v21-S3
 * ================================================================
 * Animazioni pressione pulsanti disabilitate (transition_duration 0).
 * ================================================================
 */

#include "ui.h"
#include "hardware.h"
#include <Arduino.h>
#include "autotune.h"
#include "debug_config.h"
#if SIMULATOR_MODE
#include "simulator.h"
#endif

static void mark_dirty() {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
}

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
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    if (g_state.sensor_mode != SensorMode::SINGLE) {
      g_state.set_base = fmax(50.0, g_state.set_base - 5.0);
    }
    MUTEX_GIVE();
  }
  refresh_active();
}
void cb_base_plus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    if (g_state.sensor_mode != SensorMode::SINGLE) {
      g_state.set_base = fmin(500.0, g_state.set_base + 5.0);
    }
    MUTEX_GIVE();
  }
  refresh_active();
}
void cb_cielo_minus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.set_cielo = fmax(50.0, g_state.set_cielo - 5.0);
    if (g_state.sensor_mode == SensorMode::SINGLE) g_state.set_base = g_state.set_cielo;
    MUTEX_GIVE();
  }
  refresh_active();
}
void cb_cielo_plus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.set_cielo = fmin(500.0, g_state.set_cielo + 5.0);
    if (g_state.sensor_mode == SensorMode::SINGLE) g_state.set_base = g_state.set_cielo;
    MUTEX_GIVE();
  }
  refresh_active();
}

// ================================================================
//  CALLBACKS — ON/OFF
// ================================================================
void cb_toggle_base(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    if (g_state.sensor_mode == SensorMode::SINGLE) {
      bool ns = !g_state.base_enabled;
      g_state.base_enabled = g_state.cielo_enabled = ns;
    } else {
      g_state.base_enabled = !g_state.base_enabled;
    }
#if SIMULATOR_MODE
    if (!g_state.base_enabled && !g_state.cielo_enabled)
      simulator_user_turned_heat_off();
#endif
    MUTEX_GIVE();
  }
  ui_refresh(&g_state);
  ui_refresh_temp(&g_state);
  ui_timer_auto_start();
}
void cb_toggle_cielo(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    if (g_state.sensor_mode == SensorMode::SINGLE) {
      bool ns = !g_state.cielo_enabled;
      g_state.base_enabled = g_state.cielo_enabled = ns;
    } else {
      g_state.cielo_enabled = !g_state.cielo_enabled;
    }
#if SIMULATOR_MODE
    if (!g_state.base_enabled && !g_state.cielo_enabled)
      simulator_user_turned_heat_off();
#endif
    MUTEX_GIVE();
  }
  ui_refresh(&g_state);
  ui_refresh_temp(&g_state);
  ui_timer_auto_start();
}
void cb_toggle_luce(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.luce_on = !g_state.luce_on;
    MUTEX_GIVE();
  }
  ui_refresh(&g_state); ui_refresh_temp(&g_state);
}

// ================================================================
//  CALLBACKS — mode/split
// ================================================================
void cb_toggle_mode(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    if (g_state.sensor_mode == SensorMode::SINGLE) {
      g_state.sensor_mode = SensorMode::DUAL;
      g_state.pct_base = 100;
      g_state.pct_cielo = 100;
    } else {
      g_state.sensor_mode = SensorMode::SINGLE;
      g_state.cielo_enabled = false;
    }
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
  refresh_active();
}

void cb_mode_long_press(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    bool was_single = (g_state.sensor_mode == SensorMode::SINGLE);
    g_state.sensor_mode = was_single ? SensorMode::DUAL : SensorMode::SINGLE;
    if (g_state.sensor_mode == SensorMode::SINGLE) {
        g_state.cielo_enabled = false;
    } else {
        g_state.pct_base = 100;
        g_state.pct_cielo = 100;
    }
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
  refresh_active();
}

void cb_pct_base_minus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.pct_base = max(0, g_state.pct_base - 5); g_state.nvs_dirty = true; MUTEX_GIVE(); }
  refresh_active();
}
void cb_pct_base_plus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.pct_base = min(100, g_state.pct_base + 5); g_state.nvs_dirty = true; MUTEX_GIVE(); }
  refresh_active();
}
void cb_pct_cielo_minus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.pct_cielo = max(0, g_state.pct_cielo - 5); g_state.nvs_dirty = true; MUTEX_GIVE(); }
  refresh_active();
}
void cb_pct_cielo_plus(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.pct_cielo = min(100, g_state.pct_cielo + 5); g_state.nvs_dirty = true; MUTEX_GIVE(); }
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
void cb_goto_autotune(lv_event_t*)        { ui_show_screen(Screen::AUTOTUNE);  }

// ================================================================
//  CALLBACKS — grafico preset
// ================================================================
static void set_preset(int minutes) {
  g_graph_minutes = minutes;
  lv_color_t ca = UI_COL_GREEN;
  lv_color_t ci = UI_COL_SURFACE;
  lv_obj_set_style_bg_color(ui_BtnPreset5,  minutes ==  5 ? ca : ci, 0);
  lv_obj_set_style_bg_color(ui_BtnPreset15, minutes == 15 ? ca : ci, 0);
  lv_obj_set_style_bg_color(ui_BtnPreset30, minutes == 30 ? ca : ci, 0);
  lv_color_t ta = UI_COL_BG;
  lv_color_t ti = UI_COL_GREEN;
  lv_obj_t* btns[3] = {ui_BtnPreset5, ui_BtnPreset15, ui_BtnPreset30};
  int prs[3] = {5, 15, 30};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* l = lv_obj_get_child(btns[i], 0);
    if (l) lv_obj_set_style_text_color(l, minutes == prs[i] ? ta : ti, 0);
  }
  char buf[20]; snprintf(buf, sizeof(buf), "Ultimi %d min", minutes);
  lv_label_set_text(ui_GraphTimeLbl, buf);
  ui_refresh_graph(&g_state);
}
void cb_preset_5 (lv_event_t*) { set_preset(5);  }
void cb_preset_15(lv_event_t*) { set_preset(15); }
void cb_preset_30(lv_event_t*) { set_preset(30); }

// ================================================================
//  CALLBACKS — PID tuning
// ================================================================
void cb_kp_base_m(lv_event_t*)  { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kp_base  = fmax(0.0,  g_state.kp_base  - 0.1); MUTEX_GIVE(); }   refresh_active(); }
void cb_kp_base_p(lv_event_t*)  { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kp_base  = fmin(20.0, g_state.kp_base  + 0.1); MUTEX_GIVE(); }   refresh_active(); }
void cb_ki_base_m(lv_event_t*)  { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.ki_base  = fmax(0.0,  g_state.ki_base  - 0.005); MUTEX_GIVE(); } refresh_active(); }
void cb_ki_base_p(lv_event_t*)  { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.ki_base  = fmin(1.0,  g_state.ki_base  + 0.005); MUTEX_GIVE(); } refresh_active(); }
void cb_kd_base_m(lv_event_t*)  { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kd_base  = fmax(0.0,  g_state.kd_base  - 0.1); MUTEX_GIVE(); }   refresh_active(); }
void cb_kd_base_p(lv_event_t*)  { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kd_base  = fmin(20.0, g_state.kd_base  + 0.1); MUTEX_GIVE(); }   refresh_active(); }
void cb_kp_cielo_m(lv_event_t*) { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kp_cielo = fmax(0.0,  g_state.kp_cielo - 0.1); MUTEX_GIVE(); }   refresh_active(); }
void cb_kp_cielo_p(lv_event_t*) { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kp_cielo = fmin(20.0, g_state.kp_cielo + 0.1); MUTEX_GIVE(); }   refresh_active(); }
void cb_ki_cielo_m(lv_event_t*) { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.ki_cielo = fmax(0.0,  g_state.ki_cielo - 0.005); MUTEX_GIVE(); } refresh_active(); }
void cb_ki_cielo_p(lv_event_t*) { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.ki_cielo = fmin(1.0,  g_state.ki_cielo + 0.005); MUTEX_GIVE(); } refresh_active(); }
void cb_kd_cielo_m(lv_event_t*) { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kd_cielo = fmax(0.0,  g_state.kd_cielo - 0.1); MUTEX_GIVE(); }   refresh_active(); }
void cb_kd_cielo_p(lv_event_t*) { if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) { g_state.kd_cielo = fmin(20.0, g_state.kd_cielo + 0.1); MUTEX_GIVE(); }   refresh_active(); }

void cb_pid_save(lv_event_t*) {
  if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
    g_state.nvs_dirty = true;
    MUTEX_GIVE();
  }
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
#define TIMER_DEFAULT_MIN  10
static int  s_timer_minutes = TIMER_DEFAULT_MIN;
static int  s_timer_seconds = 0;
static bool s_timer_running = false;

static void timer_refresh_display() {
  if (!ui_LabelTimerValue) return;
  char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", s_timer_minutes, s_timer_seconds);
  lv_label_set_text(ui_LabelTimerValue, buf);
  if (ui_LabelTimerStatus)
    lv_label_set_text(ui_LabelTimerStatus, s_timer_running ? LV_SYMBOL_PLAY " IN CORSO" : "");
}

void cb_timer_plus(lv_event_t*)   { if (!s_timer_running) { s_timer_minutes = min(s_timer_minutes + 1,  99); s_timer_seconds = 0; timer_refresh_display(); } }
void cb_timer_minus(lv_event_t*)  { if (!s_timer_running) { s_timer_minutes = max(s_timer_minutes - 1,   1); s_timer_seconds = 0; timer_refresh_display(); } }
void cb_timer_plus15(lv_event_t*) { if (!s_timer_running) { s_timer_minutes = min(s_timer_minutes + 15, 99); s_timer_seconds = 0; timer_refresh_display(); } }
void cb_timer_minus15(lv_event_t*){ if (!s_timer_running) { s_timer_minutes = max(s_timer_minutes - 15,  1); s_timer_seconds = 0; timer_refresh_display(); } }
void cb_timer_reset(lv_event_t*)  {
  s_timer_running = false;
  s_timer_minutes = TIMER_DEFAULT_MIN;
  s_timer_seconds = 0;
  timer_refresh_display();
  if (ui_LabelTimerStatus) lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_CHARGE " Parte con resistenza ON");
}

// ================================================================
//  TIMER DI SICUREZZA — API chiamate da altri task
// ================================================================
void ui_timer_auto_start(void) {
  if (s_timer_running) return;
  if (!(g_state.base_enabled || g_state.cielo_enabled)) return;
  s_timer_running = true;
  if (ui_LabelTimerStatus)
    lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_PLAY " IN CORSO");
}

void ui_timer_tick_1s(void) {
  if (!s_timer_running) return;
  if (s_timer_minutes == 0 && s_timer_seconds == 0) return;

  if (s_timer_seconds == 0) {
    if (s_timer_minutes == 0) return;
    s_timer_minutes--;
    s_timer_seconds = 59;
  } else {
    s_timer_seconds--;
  }

  timer_refresh_display();

  if (s_timer_minutes == 0 && s_timer_seconds == 0) {
    s_timer_running = false;
    // Spegni resistenze — la ventola resta gestita dalla sua logica temperatura/relay
    if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
      g_state.base_enabled  = false;
      g_state.cielo_enabled = false;
      MUTEX_GIVE();
    }
    if (ui_LabelTimerStatus)
      lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_STOP " Fine cottura — resistenze OFF");
  }
}

// ================================================================
//  CALLBACK — AVVIO AUTOTUNE PID
// ================================================================
void cb_autotune_start(lv_event_t*) {
  if (g_state.safety_shutdown) return;
  if (autotune_is_running())   return;

  autotune_apply_default_split();
  autotune_start();
}

void cb_autotune_stop(lv_event_t*) {
  if (!autotune_is_running()) return;
  autotune_stop();
}

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
