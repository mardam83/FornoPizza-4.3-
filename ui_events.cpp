/**
 * ui_events.cpp — Callback eventi + ui_refresh v17
 * ================================================================
 * Architettura 5 schermate:
 *   MAIN      → ui_refresh()        — aggiorna archi, LED, PID%
 *   TEMP      → ui_refresh_temp()   — aggiorna setpoint, ON/OFF, split
 *   PID_BASE  → ui_refresh_pid()    — aggiorna valori Kp/Ki/Kd base
 *   PID_CIELO → ui_refresh_pid()    — aggiorna valori Kp/Ki/Kd cielo
 *   GRAPH     → ui_refresh_graph()  — grafico storico
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
//  CALLBACKS — setpoint (ora in TEMP)
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
//  CALLBACKS — ON/OFF (usati sia da MAIN che da TEMP)
// ================================================================
void cb_toggle_base(lv_event_t*) {
  if (g_state.sensor_mode == SensorMode::SINGLE) {
    bool ns = !g_state.base_enabled;
    g_state.base_enabled = g_state.cielo_enabled = ns;
  } else {
    g_state.base_enabled = !g_state.base_enabled;
  }
  // Aggiorna entrambe le schermate che mostrano ON/OFF
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
//  CALLBACKS — mode/split (ora in TEMP)
// ================================================================
void cb_toggle_mode(lv_event_t*) {
  // Tocco breve: alterna SINGLE ↔ DUAL
  if (g_state.sensor_mode == SensorMode::SINGLE) {
    g_state.sensor_mode = SensorMode::DUAL;
  } else {
    g_state.sensor_mode = SensorMode::SINGLE;
    // In SINGLE: disabilita cielo, mantieni solo base
    g_state.cielo_enabled = false;
  }
  g_state.nvs_dirty = true;
  refresh_active();
}

void cb_mode_long_press(lv_event_t*) {
  // Pressione lunga: apre selezione sonda (mostra popup LVGL msgbox)
  // Per ora alterna anche qui — una UI popup completa richiederebbe
  // un lv_msgbox dedicato che può essere aggiunto in seguito
  // Qui per semplicità: forza BASE-only in SINGLE, o attiva entrambe in DUAL
  bool was_single = (g_state.sensor_mode == SensorMode::SINGLE);
  g_state.sensor_mode = was_single ? SensorMode::DUAL : SensorMode::SINGLE;
  if (g_state.sensor_mode == SensorMode::SINGLE)
    g_state.cielo_enabled = false;
  g_state.nvs_dirty = true;
  refresh_active();
}
// ── Percentuale potenza BASE (0-100%) — indipendente, passo 5%
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
// ── Percentuale potenza CIELO (0-100%) — indipendente, passo 5%
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
void cb_goto_pid_base(lv_event_t*)        { ui_show_screen(Screen::PID_BASE);   }
void cb_goto_pid_cielo(lv_event_t*)       { ui_show_screen(Screen::PID_CIELO);  }
void cb_goto_graph(lv_event_t*)           { ui_show_screen(Screen::GRAPH);      }
void cb_goto_main_from_graph(lv_event_t*) { ui_show_screen(Screen::MAIN);      }

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
  int  prs[3] = {5, 15, 30};
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
void cb_kp_base_m(lv_event_t*) { g_state.kp_base  = fmax(0.0,  g_state.kp_base  - 0.1);   refresh_active(); }
void cb_kp_base_p(lv_event_t*) { g_state.kp_base  = fmin(20.0, g_state.kp_base  + 0.1);   refresh_active(); }
void cb_ki_base_m(lv_event_t*) { g_state.ki_base  = fmax(0.0,  g_state.ki_base  - 0.005); refresh_active(); }
void cb_ki_base_p(lv_event_t*) { g_state.ki_base  = fmin(1.0,  g_state.ki_base  + 0.005); refresh_active(); }
void cb_kd_base_m(lv_event_t*) { g_state.kd_base  = fmax(0.0,  g_state.kd_base  - 0.1);   refresh_active(); }
void cb_kd_base_p(lv_event_t*) { g_state.kd_base  = fmin(20.0, g_state.kd_base  + 0.1);   refresh_active(); }
void cb_kp_cielo_m(lv_event_t*){ g_state.kp_cielo = fmax(0.0,  g_state.kp_cielo - 0.1);   refresh_active(); }
void cb_kp_cielo_p(lv_event_t*){ g_state.kp_cielo = fmin(20.0, g_state.kp_cielo + 0.1);   refresh_active(); }
void cb_ki_cielo_m(lv_event_t*){ g_state.ki_cielo = fmax(0.0,  g_state.ki_cielo - 0.005); refresh_active(); }
void cb_ki_cielo_p(lv_event_t*){ g_state.ki_cielo = fmin(1.0,  g_state.ki_cielo + 0.005); refresh_active(); }
void cb_kd_cielo_m(lv_event_t*){ g_state.kd_cielo = fmax(0.0,  g_state.kd_cielo - 0.1);   refresh_active(); }
void cb_kd_cielo_p(lv_event_t*){ g_state.kd_cielo = fmin(20.0, g_state.kd_cielo + 0.1);   refresh_active(); }

void cb_pid_save(lv_event_t*) {
  g_state.nvs_dirty = true;
  // Aggiorna label salvataggio nella schermata attiva
  lv_obj_t* lbl = (g_state.active_screen == Screen::PID_CIELO)
                   ? ui_LblSaveStatusCielo : ui_LblSaveStatus;
  lv_label_set_text(lbl, LV_SYMBOL_OK " Salvato!");
  lv_obj_set_style_text_color(lbl, UI_COL_GREEN, 0);
  lv_timer_t* t = lv_timer_create([](lv_timer_t* tmr) {
    lv_label_set_text(ui_LblSaveStatus,       "");
    lv_label_set_text(ui_LblSaveStatusCielo,  "");
    lv_timer_del(tmr);
  }, 2000, nullptr);
}

// ================================================================
//  ui_refresh — MAIN (temperatura corrente sola lettura)
//  In v18: MAIN non ha più archi. Ha font48 + barra PID + bottoni ON/OFF compatti.
// ================================================================
void ui_refresh(AppState* s) {
  if (!s) return;
  char buf[64];
  bool single = (s->sensor_mode == SensorMode::SINGLE);

  // ---- Temperatura BASE (font48) ----
  if (s->tc_base_err && !single) {
    lv_label_set_text(ui_LabelErrBase, LV_SYMBOL_WARNING " ERR TC");
    lv_label_set_text(ui_LabelTempBase, "");
  } else {
    lv_label_set_text(ui_LabelErrBase, "");
    double t  = single ? s->temp_cielo : s->temp_base;
    double sp = single ? s->set_cielo  : s->set_base;
    snprintf(buf, sizeof(buf), "%.0f", t);
    lv_label_set_text(ui_LabelTempBase, buf);
    lv_obj_set_style_text_color(ui_LabelTempBase, ui_temp_color(t, sp), 0);
  }
  lv_bar_set_value(ui_BarPidBase, (int32_t)s->pid_out_base, LV_ANIM_ON);
  if (s->preheat_base)
    lv_label_set_text(ui_LabelPidBase, LV_SYMBOL_CHARGE " PREHEAT");
  else
    { snprintf(buf, sizeof(buf), "PID  %.0f%%", s->pid_out_base);
      lv_label_set_text(ui_LabelPidBase, buf); }
  if (s->relay_base) lv_led_on(ui_LedRelayBase); else lv_led_off(ui_LedRelayBase);

  // ---- Temperatura CIELO (font48) ----
  if (s->tc_cielo_err) {
    lv_label_set_text(ui_LabelErrCielo, LV_SYMBOL_WARNING " ERR TC");
    lv_label_set_text(ui_LabelTempCielo, "");
  } else {
    lv_label_set_text(ui_LabelErrCielo, "");
    snprintf(buf, sizeof(buf), "%.0f", s->temp_cielo);
    lv_label_set_text(ui_LabelTempCielo, buf);
    lv_obj_set_style_text_color(ui_LabelTempCielo,
      ui_temp_color(s->temp_cielo, s->set_cielo), 0);
  }
  lv_bar_set_value(ui_BarPidCielo, (int32_t)s->pid_out_cielo, LV_ANIM_ON);
  if (s->preheat_cielo)
    lv_label_set_text(ui_LabelPidCielo, LV_SYMBOL_CHARGE " PREHEAT");
  else
    { snprintf(buf, sizeof(buf), "PID  %.0f%%", s->pid_out_cielo);
      lv_label_set_text(ui_LabelPidCielo, buf); }
  if (s->relay_cielo) lv_led_on(ui_LedRelayCielo); else lv_led_off(ui_LedRelayCielo);

  // ---- Bottoni ON/OFF compatti (una riga, testo breve) ----
  bool bon = s->base_enabled;
  bool con = s->cielo_enabled;

  // BASE button — testo compatto "BASE ON" / "BASE OFF"
  lv_obj_set_style_bg_color(ui_BtnEnBase,
    bon ? lv_color_make(0x3A,0x18,0x00) : UI_COL_DARKGRAY, 0);
  lv_obj_set_style_border_color(ui_BtnEnBase,
    bon ? UI_COL_ACCENT    : UI_COL_GRAY, 0);
  { lv_color_t _tc = bon ? lv_color_make(0xFF,0x6B,0x00) : UI_COL_GRAY;
    lv_obj_set_style_text_color(ui_BtnEnBase,   _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_BtnEnBase,   _tc, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui_BtnEnBase,   _tc, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ui_BtnEnBase,   _tc, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui_LabelBtnBase, _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelBtnBase, _tc, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui_LabelBtnBase, _tc, LV_PART_MAIN | LV_STATE_FOCUSED); }
  lv_label_set_text(ui_LabelBtnBase,
    single ? (bon ? "FORNO ON" : "FORNO OFF")
           : (bon ? "BASE ON"  : "BASE OFF"));

  // CIELO button
  lv_obj_set_style_bg_color(ui_BtnEnCielo,
    con ? lv_color_make(0x28,0x00,0x00) : UI_COL_DARKGRAY, 0);
  lv_obj_set_style_border_color(ui_BtnEnCielo,
    con ? UI_COL_CIELO     : UI_COL_GRAY, 0);
  { lv_color_t _tc = con ? lv_color_make(0xFF,0x30,0x30) : UI_COL_GRAY;
    lv_obj_set_style_text_color(ui_BtnEnCielo,    _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_BtnEnCielo,    _tc, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui_BtnEnCielo,    _tc, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ui_BtnEnCielo,    _tc, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui_LabelBtnCielo, _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelBtnCielo, _tc, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui_LabelBtnCielo, _tc, LV_PART_MAIN | LV_STATE_FOCUSED); }
  if (single) {
    snprintf(buf, sizeof(buf), "C %d%%", s->pct_cielo);
    lv_label_set_text(ui_LabelBtnCielo, buf);
  } else {
    lv_label_set_text(ui_LabelBtnCielo, con ? "CIELO ON" : "CIELO OFF");
  }

  // LUCE button
  lv_obj_set_style_bg_color(ui_BtnLuce,
    s->luce_on ? lv_color_make(0x20,0x18,0x00) : UI_COL_DARKGRAY, 0);
  lv_obj_set_style_border_color(ui_BtnLuce,
    s->luce_on ? UI_COL_LUCE      : UI_COL_GRAY, 0);
  { lv_color_t _tc = s->luce_on ? lv_color_make(0xFF,0xE0,0x00) : UI_COL_GRAY;
    lv_obj_set_style_text_color(ui_BtnLuce,      _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_BtnLuce,      _tc, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui_BtnLuce,      _tc, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ui_BtnLuce,      _tc, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui_LabelBtnLuce, _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LabelBtnLuce, _tc, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui_LabelBtnLuce, _tc, LV_PART_MAIN | LV_STATE_FOCUSED); }
  lv_label_set_text(ui_LabelBtnLuce, s->luce_on ? "LUCE ON" : "LUCE OFF");

  // ---- VENTOLA ----
  if (s->fan_on) lv_led_on(ui_LedFan); else lv_led_off(ui_LedFan);

  // ---- SAFETY BANNER ----
  if (s->safety_shutdown) {
    lv_obj_clear_flag(ui_PanelSafety, LV_OBJ_FLAG_HIDDEN);
    const char* reasons[] = {
      "", LV_SYMBOL_WARNING " ERR TERMOCOPPIA",
      LV_SYMBOL_WARNING " SOVRATEMPERATURA",
      LV_SYMBOL_WARNING " RUNAWAY- (res. rotta?)",
      LV_SYMBOL_WARNING " RUNAWAY+",
      LV_SYMBOL_WARNING " WATCHDOG"
    };
    int idx = (int)s->safety_reason;
    if (idx >= 0 && idx <= 5) lv_label_set_text(ui_LabelSafety, reasons[idx]);
  } else {
    lv_obj_add_flag(ui_PanelSafety, LV_OBJ_FLAG_HIDDEN);
  }

  // ---- STATUS BAR ----
  if (s->tc_cielo_err) {
    lv_label_set_text(ui_LabelStatus, LV_SYMBOL_WARNING " ERR TC Cielo!");
    lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_CIELO, 0);
  } else if (!single && s->tc_base_err) {
    lv_label_set_text(ui_LabelStatus, LV_SYMBOL_WARNING " ERR TC Base!");
    lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_ACCENT, 0);
  } else {
    bool ph_any = s->preheat_base || s->preheat_cielo;
    if (ph_any) {
      // Preheat attivo: barra ciano lampeggiante
      if (single)
        snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE " PREHEAT — T:%.0f → SP:%.0f",
          s->temp_cielo, s->set_cielo);
      else
        snprintf(buf, sizeof(buf), LV_SYMBOL_CHARGE " PREHEAT%s%s — B:%.0f C:%.0f",
          s->preheat_base  ? " BASE"  : "",
          s->preheat_cielo ? " CIELO" : "",
          s->temp_base, s->temp_cielo);
      lv_label_set_text(ui_LabelStatus, buf);
      lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_CYAN, 0);
    } else {
      if (single)
        snprintf(buf, sizeof(buf), "SINGLE | T:%.0f SP:%.0f | B:%d%% C:%d%% %s",
          s->temp_cielo, s->set_cielo, s->pct_base, s->pct_cielo,
          s->nvs_dirty ? "[*]" : "");
      else
        snprintf(buf, sizeof(buf), "DUAL | B:%.0f/%.0f  C:%.0f/%.0f %s",
          s->temp_base, s->set_base, s->temp_cielo, s->set_cielo,
          s->nvs_dirty ? "[*]" : "");
      lv_label_set_text(ui_LabelStatus, buf);
      lv_obj_set_style_text_color(ui_LabelStatus, UI_COL_GRAY, 0);
    }
  }
}

// ================================================================
//  ui_refresh_temp — CTRL (archi setpoint + ±5°C + ON/OFF + mode/split)
//  In v18: gli archi sono in questa schermata. Bottoni ON/OFF h=64 → testo multiriga.
// ================================================================
void ui_refresh_temp(AppState* s) {
  if (!s) return;
  char buf[48];
  bool single = (s->sensor_mode == SensorMode::SINGLE);

  // ---- Arco + Setpoint BASE ----
  if (s->tc_base_err && !single) {
    lv_label_set_text(ui_TempSetBase, "ERR");
    lv_obj_set_style_text_color(ui_TempSetBase, UI_COL_CIELO, 0);
    lv_label_set_text(ui_TempErrBase, LV_SYMBOL_WARNING " Sensore Base");
    lv_arc_set_value(ui_ArcBase, 0);
    lv_obj_set_style_arc_color(ui_ArcBase, UI_COL_CIELO, LV_PART_INDICATOR);
  } else {
    lv_label_set_text(ui_TempErrBase, "");
    double sp = single ? s->set_cielo : s->set_base;
    double t  = single ? s->temp_cielo : s->temp_base;
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0""C", sp);
    lv_label_set_text(ui_TempSetBase, buf);
    lv_obj_set_style_text_color(ui_TempSetBase, UI_COL_YELLOW, 0);
    lv_bar_set_value(ui_TempBarBase, (int32_t)s->pid_out_base, LV_ANIM_ON);
    lv_arc_set_value(ui_ArcBase, (int32_t)t);
    lv_obj_set_style_arc_color(ui_ArcBase, ui_temp_color(t, sp), LV_PART_INDICATOR);
  }

  // ---- Arco + Setpoint CIELO ----
  if (s->tc_cielo_err) {
    lv_label_set_text(ui_TempSetCielo, "ERR");
    lv_obj_set_style_text_color(ui_TempSetCielo, UI_COL_CIELO, 0);
    lv_label_set_text(ui_TempErrCielo, LV_SYMBOL_WARNING " Sensore Cielo");
    lv_arc_set_value(ui_ArcCielo, 0);
    lv_obj_set_style_arc_color(ui_ArcCielo, UI_COL_CIELO, LV_PART_INDICATOR);
  } else {
    lv_label_set_text(ui_TempErrCielo, "");
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0""C", s->set_cielo);
    lv_label_set_text(ui_TempSetCielo, buf);
    lv_obj_set_style_text_color(ui_TempSetCielo, UI_COL_YELLOW, 0);
    lv_bar_set_value(ui_TempBarCielo, (int32_t)s->pid_out_cielo, LV_ANIM_ON);
    lv_arc_set_value(ui_ArcCielo, (int32_t)s->temp_cielo);
    lv_obj_set_style_arc_color(ui_ArcCielo,
      ui_temp_color(s->temp_cielo, s->set_cielo), LV_PART_INDICATOR);
  }

  // ---- Bottoni ON/OFF (h=64, testo multiriga con simbolo) ----
  bool bon = s->base_enabled;
  bool con = s->cielo_enabled;

  if (bon) {
    lv_obj_set_style_bg_color(ui_TempBtnEnBase,     lv_color_make(0x3A,0x18,0x00), 0);
    lv_obj_set_style_border_color(ui_TempBtnEnBase, UI_COL_ACCENT,   0);
    { lv_color_t _tc = lv_color_make(0xFF,0x6B,0x00);
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_PRESSED);
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_FOCUSED);
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_CHECKED);
      lv_obj_set_style_text_color(ui_TempLblBtnBase, _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_color(ui_TempLblBtnBase, _tc, LV_PART_MAIN | LV_STATE_PRESSED);
      lv_obj_set_style_text_color(ui_TempLblBtnBase, _tc, LV_PART_MAIN | LV_STATE_FOCUSED); }
    lv_label_set_text(ui_TempLblBtnBase,
      single ? LV_SYMBOL_PLAY "\nFORNO\nON" : LV_SYMBOL_PLAY "\nBASE\nON");
  } else {
    lv_obj_set_style_bg_color(ui_TempBtnEnBase,     UI_COL_DARKGRAY, 0);
    lv_obj_set_style_border_color(ui_TempBtnEnBase, UI_COL_GRAY,     0);
    { lv_color_t _tc = UI_COL_GRAY;
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_PRESSED);
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_FOCUSED);
      lv_obj_set_style_text_color(ui_TempBtnEnBase,  _tc, LV_PART_MAIN | LV_STATE_CHECKED);
      lv_obj_set_style_text_color(ui_TempLblBtnBase, _tc, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_color(ui_TempLblBtnBase, _tc, LV_PART_MAIN | LV_STATE_PRESSED);
      lv_obj_set_style_text_color(ui_TempLblBtnBase, _tc, LV_PART_MAIN | LV_STATE_FOCUSED); }
    lv_label_set_text(ui_TempLblBtnBase,
      single ? LV_SYMBOL_STOP "\nFORNO\nOFF" : LV_SYMBOL_STOP "\nBASE\nOFF");
  }

  if (single) {
    lv_obj_set_style_bg_color(ui_TempBtnEnCielo,
      con ? lv_color_make(0x28,0x00,0x00) : UI_COL_DARKGRAY, 0);
    lv_obj_set_style_border_color(ui_TempBtnEnCielo,
      con ? UI_COL_CIELO     : UI_COL_GRAY, 0);
    lv_obj_set_style_text_color(ui_TempLblBtnCielo,
      con ? UI_COL_CIELO     : UI_COL_GRAY, 0);
    snprintf(buf, sizeof(buf), "CIELO\n%d%%", s->pct_cielo);
    lv_label_set_text(ui_TempLblBtnCielo, buf);
  } else {
    lv_obj_set_style_bg_color(ui_TempBtnEnCielo,
      con ? lv_color_make(0x28,0x00,0x00) : UI_COL_DARKGRAY, 0);
    lv_obj_set_style_border_color(ui_TempBtnEnCielo,
      con ? UI_COL_CIELO     : UI_COL_GRAY, 0);
    lv_obj_set_style_text_color(ui_TempLblBtnCielo,
      con ? UI_COL_CIELO     : UI_COL_GRAY, 0);
    lv_label_set_text(ui_TempLblBtnCielo,
      con ? LV_SYMBOL_PLAY "\nCIELO\nON" : LV_SYMBOL_STOP "\nCIELO\nOFF");
  }

  if (s->luce_on) {
    lv_obj_set_style_bg_color(ui_TempBtnLuce,      lv_color_make(0x20,0x18,0x00), 0);
    lv_obj_set_style_border_color(ui_TempBtnLuce,  UI_COL_LUCE,      0);
    lv_obj_set_style_text_color(ui_TempLblBtnLuce, UI_COL_LUCE,      0);
    lv_label_set_text(ui_TempLblBtnLuce, LV_SYMBOL_CHARGE "\nLUCE\nON");
  } else {
    lv_obj_set_style_bg_color(ui_TempBtnLuce,      UI_COL_DARKGRAY, 0);
    lv_obj_set_style_border_color(ui_TempBtnLuce,  UI_COL_GRAY,     0);
    lv_obj_set_style_text_color(ui_TempLblBtnLuce, UI_COL_GRAY,     0);
    lv_label_set_text(ui_TempLblBtnLuce, LV_SYMBOL_CHARGE "\nLUCE\nOFF");
  }

  // ---- Mode toggle ----
  lv_color_t mcol = single ? UI_COL_SINGLE : UI_COL_DUAL;
  lv_obj_set_style_border_color(ui_BtnModeToggle, mcol, 0);
  lv_obj_set_style_bg_color(ui_BtnModeToggle,
    single ? lv_color_make(0x20,0x00,0x32) : lv_color_make(0x00,0x20,0x32), 0);
  lv_obj_set_style_text_color(ui_LabelMode, mcol, 0);
  lv_label_set_text(ui_LabelMode,
    single ? LV_SYMBOL_PAUSE " SINGLE" : LV_SYMBOL_LOOP " DUAL");

  // ---- Split panel ----
  if (single) {
    lv_obj_clear_flag(ui_PanelSplit, LV_OBJ_FLAG_HIDDEN);
    snprintf(buf, sizeof(buf), "B %d%%", s->pct_base);
    lv_label_set_text(ui_LabelPctBase, buf);
    snprintf(buf, sizeof(buf), "C %d%%", s->pct_cielo);
    lv_label_set_text(ui_LabelPctCielo, buf);
  } else {
    lv_obj_add_flag(ui_PanelSplit, LV_OBJ_FLAG_HIDDEN);
  }

  // ---- Labels status (nascosti in CTRL ma aggiornati per coerenza) ----
  if (single)
    snprintf(buf, sizeof(buf), "SINGLE | B:%d%%  C:%d%%", s->pct_base, s->pct_cielo);
  else
    snprintf(buf, sizeof(buf), "DUAL | PID indipendenti");
  lv_label_set_text(ui_TempModeLbl, buf);
  lv_obj_set_style_text_color(ui_TempModeLbl, mcol, 0);
}

// ================================================================
//  ui_refresh_pid — PID_BASE e PID_CIELO
// ================================================================
void ui_refresh_pid(AppState* s) {
  if (!s) return;
  char buf[24];

  // PID Base
  snprintf(buf, sizeof(buf), "%.2f", s->kp_base); lv_label_set_text(ui_LblKpBase, buf);
  snprintf(buf, sizeof(buf), "%.3f", s->ki_base); lv_label_set_text(ui_LblKiBase, buf);
  snprintf(buf, sizeof(buf), "%.2f", s->kd_base); lv_label_set_text(ui_LblKdBase, buf);
  snprintf(buf, sizeof(buf), "SET: %.0f\xC2\xB0""C", s->set_base);
  lv_label_set_text(ui_LblSetBaseTun, buf);

  // PID Cielo
  snprintf(buf, sizeof(buf), "%.2f", s->kp_cielo); lv_label_set_text(ui_LblKpCielo, buf);
  snprintf(buf, sizeof(buf), "%.3f", s->ki_cielo); lv_label_set_text(ui_LblKiCielo, buf);
  snprintf(buf, sizeof(buf), "%.2f", s->kd_cielo); lv_label_set_text(ui_LblKdCielo, buf);
  snprintf(buf, sizeof(buf), "SET: %.0f\xC2\xB0""C", s->set_cielo);
  lv_label_set_text(ui_LblSetCieloTun, buf);
}

// ================================================================
//  ui_refresh_graph — grafico storico (invariato)
// ================================================================
void ui_refresh_graph(AppState* s) {
  if (!s || !ui_Chart || !ui_SerBase || !ui_SerCielo) return;

  int samples_wanted = (g_graph_minutes * 60) / GRAPH_SAMPLE_S;
  if (samples_wanted > GRAPH_BUF_SIZE) samples_wanted = GRAPH_BUF_SIZE;
  if (samples_wanted > (int)g_graph.count) samples_wanted = (int)g_graph.count;
  if (samples_wanted == 0) { lv_label_set_text(ui_GraphTimeLbl, "In attesa..."); return; }

  lv_chart_set_point_count(ui_Chart, (uint16_t)samples_wanted);

  float ymin = 9999.0f, ymax = -9999.0f;
  for (int i = 0; i < samples_wanted; i++) {
    int idx = (int)g_graph.head - samples_wanted + i;
    while (idx < 0) idx += GRAPH_BUF_SIZE;
    idx %= GRAPH_BUF_SIZE;
    float tb = g_graph.base[idx], tc = g_graph.cielo[idx];
    if (tb > 0 && tb < ymin) ymin = tb;
    if (tc > 0 && tc < ymin) ymin = tc;
    if (tb > ymax) ymax = tb;
    if (tc > ymax) ymax = tc;
  }
  if (ymin > 9000.0f) ymin = 0.0f;
  if (ymax < -9000.0f) ymax = 100.0f;

  int y_lo = (int)(ymin / 50) * 50;
  int y_hi = ((int)(ymax / 50) + 1) * 50 + 20;
  if (y_lo < 0) y_lo = 0;
  if (y_hi > 600) y_hi = 600;

  lv_chart_set_range(ui_Chart, LV_CHART_AXIS_PRIMARY_Y, y_lo, y_hi);

  char buf[10];
  snprintf(buf, sizeof(buf), "%d", y_hi); lv_label_set_text(ui_GraphMaxLbl, buf);
  snprintf(buf, sizeof(buf), "%d", y_lo); lv_label_set_text(ui_GraphMinLbl, buf);

  lv_chart_set_all_value(ui_Chart, ui_SerBase,  LV_CHART_POINT_NONE);
  lv_chart_set_all_value(ui_Chart, ui_SerCielo, LV_CHART_POINT_NONE);

  for (int i = 0; i < samples_wanted; i++) {
    int idx = (int)g_graph.head - samples_wanted + i;
    while (idx < 0) idx += GRAPH_BUF_SIZE;
    idx %= GRAPH_BUF_SIZE;
    lv_chart_set_next_value(ui_Chart, ui_SerBase,  (lv_coord_t)g_graph.base[idx]);
    lv_chart_set_next_value(ui_Chart, ui_SerCielo, (lv_coord_t)g_graph.cielo[idx]);
  }
  lv_chart_refresh(ui_Chart);
}

// ================================================================
//  CALLBACKS — WiFi Scan + OTA (v19)
// ================================================================

// ================================================================
//  CALLBACKS — TIMER (v20)
// ================================================================
// Stato timer — gestito localmente (non nell'AppState del PID)
static int  s_timer_minutes  = TIMER_DEFAULT_MIN;
static int  s_timer_seconds  = 0;
static bool s_timer_running  = false;

static void timer_refresh_display() {
    if (!ui_LabelTimerValue) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", s_timer_minutes, s_timer_seconds);
    lv_label_set_text(ui_LabelTimerValue, buf);
    if (ui_LabelTimerStatus)
        lv_label_set_text(ui_LabelTimerStatus,
            s_timer_running ? LV_SYMBOL_PLAY " IN CORSO" : "");
}

void cb_goto_timer(lv_event_t*)   { ui_show_screen(Screen::TIMER);   }
void cb_goto_ricette(lv_event_t*) { ui_show_screen(Screen::RICETTE); }

// Timer parte automaticamente quando una resistenza viene accesa (vedi Task_PID)
// L'utente può solo regolare il valore e resettare.

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

// ================================================================
//  CALLBACKS — RICETTE (v20)
// ================================================================
// Struttura ricetta (deve corrispondere a quella in ui.cpp)
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
    // Applica solo setpoint — il timer non viene toccato
    if (MUTEX_TAKE_MS(MUTEX_TIMEOUT_MS)) {
        g_state.set_base   = r.set_base;
        g_state.set_cielo  = r.set_cielo;
        g_state.nvs_dirty  = true;
        MUTEX_GIVE();
    }
    // Torna a CTRL per conferma visiva
    ui_show_screen(Screen::TEMP);
}


// ================================================================
//  TIMER — auto-start quando resistenza ON (chiamato da Task_PID)
// ================================================================
void ui_timer_auto_start() {
    if (s_timer_running) return;  // già in corso
    if (s_timer_minutes == 0 && s_timer_seconds == 0) return;
    s_timer_running = true;
    timer_refresh_display();
    if (ui_LabelTimerStatus)
        lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_PLAY " IN CORSO");
}
void ui_timer_tick_1s() {
    // Da chiamare ogni secondo quando il timer è in corso
    if (!s_timer_running) return;
    if (s_timer_seconds > 0) {
        s_timer_seconds--;
    } else if (s_timer_minutes > 0) {
        s_timer_minutes--;
        s_timer_seconds = 59;
    } else {
        s_timer_running = false;
        if (ui_LabelTimerStatus)
            lv_label_set_text(ui_LabelTimerStatus, LV_SYMBOL_OK " TEMPO SCADUTO!");
    }
    timer_refresh_display();
}

void cb_goto_wifi(lv_event_t*) {
  ui_show_screen(Screen::WIFI_SCAN);
}

void cb_goto_ota(lv_event_t*) {
  ui_show_screen(Screen::OTA);
}
