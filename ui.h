/**
 * ui.h — Forno Pizza v17 — FIX PSRAM
 * ================================================================
 * FIX v2:
 *   - GraphBuffer spostato in PSRAM (360 × 2 × 4 byte = ~2.8KB liberati da SRAM)
 *   - g_graph ora è un puntatore allocato in setup() via graph_alloc_psram()
 *   - Tutti gli accessi a g_graph invariati (compatibilità totale)
 * ================================================================
 */
#pragma once
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "nvs_storage.h"
#include "ui_wifi.h"

// ----------------------------------------------------------------
//  ENUMERAZIONI
// ----------------------------------------------------------------
enum class AutotuneStatus { IDLE=0, RUNNING=1, DONE=2, ABORTED=3 };
enum class SafetyReason   { NONE=0, TC_ERROR=1, OVERTEMP=2,
                            RUNAWAY_DOWN=3, RUNAWAY_UP=4, WDG_TIMEOUT=5 };
enum class SensorMode     { SINGLE, DUAL };
enum class Screen         { MAIN, TEMP, PID_BASE, PID_CIELO, GRAPH,
                            WIFI_SCAN, WIFI_PWD, OTA, TIMER, RICETTE };

// ----------------------------------------------------------------
//  STATO APPLICAZIONE — in SRAM interna (accesso real-time da Task_PID)
// ----------------------------------------------------------------
struct AppState {
  double  temp_base,     temp_cielo;
  double  set_base,      set_cielo;
  double  pid_out_base,  pid_out_cielo;
  double  kp_base,  ki_base,  kd_base;
  double  kp_cielo, ki_cielo, kd_cielo;
  SensorMode sensor_mode;
  int     pct_base;
  int     pct_cielo;
  bool    base_enabled, cielo_enabled, luce_on;
  bool    relay_base,   relay_cielo,   fan_on;
  bool    preheat_base, preheat_cielo;
  bool    tc_base_err,  tc_cielo_err;
  bool    safety_shutdown;
  SafetyReason safety_reason;
  AutotuneStatus autotune_status;
  int     autotune_split, autotune_cycles;
  float   autotune_kp, autotune_ki, autotune_kd;
  int     timer_minutes;
  bool    timer_running;
  Screen  active_screen;
  bool    nvs_dirty;
};
extern AppState g_state;

// ----------------------------------------------------------------
//  RING BUFFER GRAFICO — allocato in PSRAM via graph_alloc_psram()
//  360 campioni × 2 array float × 4 byte = 2.880 byte in PSRAM
//  head e count rimangono in SRAM (accesso frequente, 4 byte totali)
// ----------------------------------------------------------------
#define TIMER_DEFAULT_MIN  10
#define GRAPH_SAMPLE_S     5
#define GRAPH_MAX_MINUTES  30
#define GRAPH_BUF_SIZE     (GRAPH_MAX_MINUTES * 60 / GRAPH_SAMPLE_S)  // 360

struct GraphBuffer {
  float*   base;   // puntatore → array in PSRAM
  float*   cielo;  // puntatore → array in PSRAM
  uint16_t head;
  uint16_t count;
};
extern GraphBuffer g_graph;

// Alloca gli array base/cielo in PSRAM — chiama da setup() DOPO display_init()
inline bool graph_alloc_psram() {
  g_graph.base  = (float*)heap_caps_malloc(GRAPH_BUF_SIZE * sizeof(float),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_graph.cielo = (float*)heap_caps_malloc(GRAPH_BUF_SIZE * sizeof(float),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!g_graph.base || !g_graph.cielo) {
    // Fallback SRAM interna
    Serial.println("[GRAPH] WARN: PSRAM non disponibile — fallback SRAM");
    static float fb[GRAPH_BUF_SIZE];
    static float fc[GRAPH_BUF_SIZE];
    if (g_graph.base)  { heap_caps_free(g_graph.base);  g_graph.base  = nullptr; }
    if (g_graph.cielo) { heap_caps_free(g_graph.cielo); g_graph.cielo = nullptr; }
    g_graph.base  = fb;
    g_graph.cielo = fc;
  }
  g_graph.head  = 0;
  g_graph.count = 0;
  memset(g_graph.base,  0, GRAPH_BUF_SIZE * sizeof(float));
  memset(g_graph.cielo, 0, GRAPH_BUF_SIZE * sizeof(float));
  Serial.printf("[GRAPH] Buffer %d campioni × 2 — %s\n",
                GRAPH_BUF_SIZE,
                heap_caps_check_integrity_addr((intptr_t)g_graph.base, false)
                  ? "PSRAM" : "SRAM");
  return (g_graph.base != nullptr && g_graph.cielo != nullptr);
}

// ================================================================
//  WIDGET — MAIN
// ================================================================
extern lv_obj_t* ui_ScreenMain;
extern lv_obj_t* ui_LabelTempBase;
extern lv_obj_t* ui_LabelPidBase;
extern lv_obj_t* ui_BarPidBase;
extern lv_obj_t* ui_LedRelayBase;
extern lv_obj_t* ui_LabelErrBase;
extern lv_obj_t* ui_BtnEnBase;
extern lv_obj_t* ui_LabelBtnBase;
extern lv_obj_t* ui_LabelTempCielo;
extern lv_obj_t* ui_LabelPidCielo;
extern lv_obj_t* ui_BarPidCielo;
extern lv_obj_t* ui_LedRelayCielo;
extern lv_obj_t* ui_LabelErrCielo;
extern lv_obj_t* ui_BtnEnCielo;
extern lv_obj_t* ui_LabelBtnCielo;
extern lv_obj_t* ui_LabelStatus;
extern lv_obj_t* ui_LabelSafetyMain;
extern lv_obj_t* ui_LabelFanMain;
extern lv_obj_t* ui_BtnNavTemp;
extern lv_obj_t* ui_BtnNavGraph;
extern lv_obj_t* ui_BtnNavTimer;
extern lv_obj_t* ui_BtnNavWifi;

// ================================================================
//  WIDGET — TEMP
// ================================================================
extern lv_obj_t* ui_ScreenTemp;
extern lv_obj_t* ui_ArcBase;
extern lv_obj_t* ui_ArcCielo;
extern lv_obj_t* ui_TempSetBase;
extern lv_obj_t* ui_TempSetCielo;
extern lv_obj_t* ui_TempBarBase;
extern lv_obj_t* ui_TempBarCielo;
extern lv_obj_t* ui_TempErrBase;
extern lv_obj_t* ui_TempErrCielo;
extern lv_obj_t* ui_TempBtnEnBase;
extern lv_obj_t* ui_TempLblBtnBase;
extern lv_obj_t* ui_TempBtnEnCielo;
extern lv_obj_t* ui_TempLblBtnCielo;
extern lv_obj_t* ui_TempBtnLuce;
extern lv_obj_t* ui_TempLblBtnLuce;
extern lv_obj_t* ui_BtnModeToggle;
extern lv_obj_t* ui_LabelMode;
extern lv_obj_t* ui_PanelSplit;
extern lv_obj_t* ui_LabelPctBase;
extern lv_obj_t* ui_LabelPctCielo;
extern lv_obj_t* ui_BtnPctBaseMinus;
extern lv_obj_t* ui_BtnPctBasePlus;
extern lv_obj_t* ui_BtnPctCieloMinus;
extern lv_obj_t* ui_BtnPctCieloPlus;
extern lv_obj_t* ui_TempModeLbl;
extern lv_obj_t* ui_TempStatusLbl;
extern lv_obj_t* ui_TempFanLbl;
extern lv_obj_t* ui_TempSafetyLbl;
extern lv_obj_t* ui_LabelSplitCielo;
extern lv_obj_t* ui_BtnSplitMinus;
extern lv_obj_t* ui_BtnSplitPlus;

// ================================================================
//  WIDGET — PID BASE
// ================================================================
extern lv_obj_t* ui_ScreenPidBase;
extern lv_obj_t* ui_LblKpBase;
extern lv_obj_t* ui_LblKiBase;
extern lv_obj_t* ui_LblKdBase;
extern lv_obj_t* ui_LblSetBaseTun;
extern lv_obj_t* ui_LblSaveStatus;

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
//  COLORI UI — definiti qui così sono visibili a ui.cpp, ui_events.cpp, ecc.
// ================================================================
#define UI_COL_BG        lv_color_make(0x10,0x10,0x1A)
#define UI_COL_ACCENT    lv_color_make(0xFF,0x6B,0x00)   // arancio BASE
#define UI_COL_ACCENT2   lv_color_make(0xFF,0x9E,0x40)   // arancio chiaro (preheating)
#define UI_COL_CIELO     lv_color_make(0xFF,0x30,0x30)   // rosso CIELO
#define UI_COL_LUCE      lv_color_make(0xFF,0xE0,0x00)   // giallo luce
#define UI_COL_TUNING    lv_color_make(0x00,0xC0,0xFF)   // azzurro PID tuning
#define UI_COL_SINGLE    lv_color_make(0xA0,0x00,0xFF)   // viola modo SINGLE
#define UI_COL_DUAL      lv_color_make(0x00,0xC0,0xFF)   // cyan modo DUAL
#define UI_COL_WHITE     lv_color_make(0xFF,0xFF,0xFF)
#define UI_COL_GRAY      lv_color_make(0x60,0x60,0x70)
#define UI_COL_DARKGRAY  lv_color_make(0x28,0x28,0x38)
#define UI_COL_GREEN     lv_color_make(0x00,0xE6,0x76)   // verde OK/save
#define UI_COL_CYAN      lv_color_make(0x00,0xD4,0xFF)   // cyan nav
#define UI_COL_YELLOW    lv_color_make(0xFF,0xE0,0x00)   // giallo setpoint

// Colore temperatura progressivo: freddo → caldo → target → sopra
inline lv_color_t ui_temp_color(double t, double sp) {
  if      (t < sp * 0.60) return lv_color_make(0x00,0x80,0xFF);  // blu freddo
  else if (t < sp * 0.85) return lv_color_make(0x00,0xD4,0xFF);  // cyan
  else if (t < sp - 5.0)  return UI_COL_YELLOW;                   // giallo vicino
  else if (t <= sp + 5.0) return UI_COL_GREEN;                    // verde a target
  else                    return lv_color_make(0xFF,0x30,0x30);   // rosso sopra
}

// ================================================================
//  API UI
// ================================================================
void ui_init();
void ui_show_screen(Screen s);
void ui_refresh(AppState* s);
void ui_refresh_temp(AppState* s);
void ui_refresh_pid(AppState* s);
void ui_refresh_graph(AppState* s);
void ui_timer_auto_start();
void ui_timer_tick_1s();

// ================================================================
//  VARIABILI GLOBALI
// ================================================================
extern volatile uint32_t g_pid_heartbeat;
extern volatile bool     g_emergency_shutdown;
void emergency_shutdown(SafetyReason reason);
