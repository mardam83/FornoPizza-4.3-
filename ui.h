/**
 * ui.h — Forno Pizza v17
 * ================================================================
 * 5 schermate:
 *   MAIN      → temperatura corrente (sola lettura) + ON/OFF + nav
 *   TEMP      → controllo setpoint ±5°C + ON/OFF + luce + mode/split
 *   PID_BASE  → tuning Kp/Ki/Kd base (schermata dedicata, layout ampio)
 *   PID_CIELO → tuning Kp/Ki/Kd cielo (schermata dedicata, layout ampio)
 *   GRAPH     → grafico storico
 * ================================================================
 */
#pragma once
#include <lvgl.h>
#include "nvs_storage.h"
#include "ui_wifi.h"   // schermate WiFi Scan + OTA (v19)

// ----------------------------------------------------------------
//  ENUMERAZIONI
// ----------------------------------------------------------------
enum class AutotuneStatus { IDLE=0, RUNNING=1, DONE=2, ABORTED=3 };
enum class SafetyReason   { NONE=0, TC_ERROR=1, OVERTEMP=2,
                            RUNAWAY_DOWN=3, RUNAWAY_UP=4, WDG_TIMEOUT=5 };
enum class SensorMode     { SINGLE, DUAL };
enum class Screen         { MAIN, TEMP, PID_BASE, PID_CIELO, GRAPH,
                            WIFI_SCAN, WIFI_PWD, OTA,
                            TIMER, RICETTE };
                            // WIFI_SCAN → scansione reti
                            // WIFI_PWD  → inserimento password
                            // OTA       → aggiornamento firmware

// ----------------------------------------------------------------
//  STATO APPLICAZIONE
// ----------------------------------------------------------------
struct AppState {
  double  temp_base,     temp_cielo;
  double  set_base,      set_cielo;
  double  pid_out_base,  pid_out_cielo;
  double  kp_base,  ki_base,  kd_base;
  double  kp_cielo, ki_cielo, kd_cielo;
  SensorMode sensor_mode;
  int     pct_base;    // 0-100% — scala output PID sulla resistenza BASE
  int     pct_cielo;   // 0-100% — scala output PID sulla resistenza CIELO
  bool    base_enabled, cielo_enabled, luce_on;
  bool    relay_base,   relay_cielo,   fan_on;
  bool    preheat_base, preheat_cielo;  // true = preriscaldo forzato attivo (relay 100%)
  bool    tc_base_err,  tc_cielo_err;
  bool    safety_shutdown;
  SafetyReason safety_reason;
  AutotuneStatus autotune_status;
  int     autotune_split, autotune_cycles;
  float   autotune_kp, autotune_ki, autotune_kd;
  int     timer_minutes;   // minuti impostati (default TIMER_DEFAULT_MIN)
  bool    timer_running;   // timer in esecuzione
  Screen  active_screen;
  bool    nvs_dirty;
};
extern AppState g_state;

// ----------------------------------------------------------------
//  RING BUFFER GRAFICO
// ----------------------------------------------------------------
#define TIMER_DEFAULT_MIN  10   // minuti default per il timer cottura
#define GRAPH_SAMPLE_S    5
#define GRAPH_MAX_MINUTES 30
#define GRAPH_BUF_SIZE    (GRAPH_MAX_MINUTES * 60 / GRAPH_SAMPLE_S)  // 360

struct GraphBuffer {
  float    base [GRAPH_BUF_SIZE];
  float    cielo[GRAPH_BUF_SIZE];
  uint16_t head, count;
};
extern GraphBuffer g_graph;

// ================================================================
//  WIDGET — MAIN  (sola visualizzazione)
//  Mostra: temperatura font48, LED relay, barre PID, ON/OFF compatto, status
// ================================================================
extern lv_obj_t* ui_ScreenMain;
// Pannello Base
extern lv_obj_t* ui_LabelTempBase;
extern lv_obj_t* ui_LabelPidBase;
extern lv_obj_t* ui_BarPidBase;
extern lv_obj_t* ui_LedRelayBase;
extern lv_obj_t* ui_LabelErrBase;
extern lv_obj_t* ui_BtnEnBase;
extern lv_obj_t* ui_LabelBtnBase;
// Pannello Cielo
extern lv_obj_t* ui_LabelTempCielo;
extern lv_obj_t* ui_LabelPidCielo;
extern lv_obj_t* ui_BarPidCielo;
extern lv_obj_t* ui_LedRelayCielo;
extern lv_obj_t* ui_LabelErrCielo;
extern lv_obj_t* ui_BtnEnCielo;
extern lv_obj_t* ui_LabelBtnCielo;
// Controlli e status
extern lv_obj_t* ui_BtnLuce;
extern lv_obj_t* ui_LabelBtnLuce;
extern lv_obj_t* ui_LedFan;
extern lv_obj_t* ui_LabelStatus;
extern lv_obj_t* ui_PanelSafety;
extern lv_obj_t* ui_LabelSafety;

// ================================================================
//  WIDGET — TEMP/CTRL  (controllo setpoint + ON/OFF + mode/split)
// ================================================================
extern lv_obj_t* ui_ScreenTemp;
// Archi setpoint (spostati da MAIN a CTRL in v18)
extern lv_obj_t* ui_ArcBase;
extern lv_obj_t* ui_ArcCielo;
// Setpoint base
extern lv_obj_t* ui_TempSetBase;
extern lv_obj_t* ui_TempBarBase;
extern lv_obj_t* ui_TempErrBase;
// Setpoint cielo
extern lv_obj_t* ui_TempSetCielo;
extern lv_obj_t* ui_TempBarCielo;
extern lv_obj_t* ui_TempErrCielo;
// ON/OFF (condivisi con MAIN tramite stesse callback)
extern lv_obj_t* ui_TempBtnEnBase;
extern lv_obj_t* ui_TempLblBtnBase;
extern lv_obj_t* ui_TempBtnEnCielo;
extern lv_obj_t* ui_TempLblBtnCielo;
extern lv_obj_t* ui_TempBtnLuce;
extern lv_obj_t* ui_TempLblBtnLuce;
// Mode/split
extern lv_obj_t* ui_BtnModeToggle;
extern lv_obj_t* ui_LabelMode;
extern lv_obj_t* ui_PanelSplit;
extern lv_obj_t* ui_LabelPctBase;    extern lv_obj_t* ui_LabelPctCielo;
extern lv_obj_t* ui_BtnPctBaseMinus; extern lv_obj_t* ui_BtnPctBasePlus;
extern lv_obj_t* ui_BtnPctCieloMinus;extern lv_obj_t* ui_BtnPctCieloPlus;
// ui_LabelSplitBase rimossa — sostituita da ui_LabelPctBase/Cielo
extern lv_obj_t* ui_LabelSplitCielo;
extern lv_obj_t* ui_BtnSplitMinus;
extern lv_obj_t* ui_BtnSplitPlus;
// Status
extern lv_obj_t* ui_TempModeLbl;
extern lv_obj_t* ui_TempStatusLbl;
extern lv_obj_t* ui_TempFanLbl;
extern lv_obj_t* ui_TempSafetyLbl;

// ================================================================
//  WIDGET — PID BASE
// ================================================================
extern lv_obj_t* ui_ScreenPidBase;
extern lv_obj_t* ui_LblKpBase;
extern lv_obj_t* ui_LblKiBase;
extern lv_obj_t* ui_LblKdBase;
extern lv_obj_t* ui_LblSetBaseTun;
extern lv_obj_t* ui_LblSaveStatus;   // condiviso: visibile nella schermata attiva

// ================================================================
//  WIDGET — PID CIELO
// ================================================================
extern lv_obj_t* ui_ScreenPidCielo;
extern lv_obj_t* ui_LblKpCielo;
extern lv_obj_t* ui_LblKiCielo;
extern lv_obj_t* ui_LblKdCielo;
extern lv_obj_t* ui_LblSetCieloTun;
extern lv_obj_t* ui_LblSaveStatusCielo;

// ================================================================
//  WIDGET — GRAPH
// ================================================================
extern lv_obj_t* ui_ScreenGraph;
extern lv_obj_t* ui_Chart;
extern lv_chart_series_t* ui_SerBase;
extern lv_chart_series_t* ui_SerCielo;
extern lv_obj_t* ui_GraphTimeLbl;
extern lv_obj_t* ui_GraphMaxLbl;
extern lv_obj_t* ui_GraphMinLbl;
extern lv_obj_t* ui_BtnPreset5;
extern lv_obj_t* ui_BtnPreset15;
extern lv_obj_t* ui_BtnPreset30;
extern int       g_graph_minutes;

// ================================================================
//  WIDGET — TIMER
// ================================================================
extern lv_obj_t* ui_ScreenTimer;
extern lv_obj_t* ui_LabelTimerValue;
extern lv_obj_t* ui_LabelTimerStatus;

// ================================================================
//  WIDGET — RICETTE
// ================================================================
extern lv_obj_t* ui_ScreenRicette;

// ----------------------------------------------------------------
//  FUNZIONI
// ----------------------------------------------------------------
void ui_init(void);
void ui_timer_auto_start(void);  // chiamare quando resistenza viene accesa
void ui_timer_tick_1s(void);     // chiamare ogni secondo (dal task orologio)
void ui_show_screen(Screen s);
void ui_refresh(AppState* s);
void ui_refresh_temp(AppState* s);
void ui_refresh_pid(AppState* s);
void ui_refresh_graph(AppState* s);

// ----------------------------------------------------------------
//  COLORI
// ----------------------------------------------------------------
#define UI_COL_BG        lv_color_make(0x10,0x10,0x1A)
#define UI_COL_ACCENT    lv_color_make(0xFF,0x6B,0x00)
#define UI_COL_ACCENT2   lv_color_make(0xFF,0x9E,0x40)
#define UI_COL_CIELO     lv_color_make(0xFF,0x30,0x30)
#define UI_COL_LUCE      lv_color_make(0xFF,0xE0,0x00)
#define UI_COL_TUNING    lv_color_make(0x00,0xC0,0xFF)
#define UI_COL_SINGLE    lv_color_make(0xA0,0x00,0xFF)
#define UI_COL_DUAL      lv_color_make(0x00,0xC0,0xFF)
#define UI_COL_WHITE     lv_color_make(0xFF,0xFF,0xFF)
#define UI_COL_GRAY      lv_color_make(0x60,0x60,0x70)
#define UI_COL_DARKGRAY  lv_color_make(0x28,0x28,0x38)
#define UI_COL_GREEN     lv_color_make(0x00,0xE6,0x76)
#define UI_COL_CYAN      lv_color_make(0x00,0xD4,0xFF)
#define UI_COL_YELLOW    lv_color_make(0xFF,0xE0,0x00)

inline lv_color_t ui_temp_color(double t, double sp) {
  if      (t < sp * 0.60) return lv_color_make(0x00,0x80,0xFF);
  else if (t < sp * 0.85) return lv_color_make(0x00,0xD4,0xFF);
  else if (t < sp - 5.0)  return UI_COL_YELLOW;
  else if (t <= sp + 5.0) return UI_COL_GREEN;
  else                    return lv_color_make(0xFF,0x30,0x30);
}
