/**
 * autotune.cpp — Forno Pizza Controller v15
 * PID Autotune via PID_AutoTune_v0 (Brett Beauregard)
 */

#include <PID_AutoTune_v0.h>
#include <Arduino.h>
#include "autotune.h"
#include "hardware.h"
#include "pid_ctrl.h"

// ================================================================
//  STATO INTERNO
// ================================================================
static double at_input  = 0.0;   // temperatura corrente (input libreria)
static double at_output = 0.0;   // output relay 0-100% (gestito dalla lib)

static PID_ATune at_tuner(&at_input, &at_output);

static bool     at_running        = false;
static float    saved_kp_base     = 0;
static float    saved_ki_base     = 0;
static float    saved_kd_base     = 0;
static float    saved_kp_cielo    = 0;
static float    saved_ki_cielo    = 0;
static float    saved_kd_cielo    = 0;
static bool     saved_base_enabled  = false;
static bool     saved_cielo_enabled = false;
static uint32_t at_start_ms       = 0;
static int      at_prev_cycles    = 0;
static bool     s_just_completed  = false;

bool autotune_consume_just_completed(void) {
  bool v = s_just_completed;
  s_just_completed = false;
  return v;
}

// ================================================================
//  autotune_apply_default_split — % Base da parzializzazione corrente
// ================================================================
void autotune_apply_default_split(void) {
  if (!MUTEX_TAKE_MS(50)) return;
  int p = g_state.pct_base;
  if (p < 5)  p = 5;
  if (p > 95) p = 95;
  g_state.autotune_split = p;
  xSemaphoreGive(g_mutex);
}

// ================================================================
//  autotune_start
// ================================================================
void autotune_start() {
  if (at_running) return;

  at_tuner.Cancel();

  // Salva parametri correnti per eventuale ripristino
  if (MUTEX_TAKE_MS(50)) {
    saved_kp_base  = g_state.kp_base;
    saved_ki_base  = g_state.ki_base;
    saved_kd_base  = g_state.kd_base;
    saved_kp_cielo = g_state.kp_cielo;
    saved_ki_cielo = g_state.ki_cielo;
    saved_kd_cielo = g_state.kd_cielo;

    saved_base_enabled  = g_state.base_enabled;
    saved_cielo_enabled = g_state.cielo_enabled;
    g_state.base_enabled  = true;
    g_state.cielo_enabled = true;

    {
      int sp = g_state.autotune_split;
      if (sp < 5 || sp > 95) {
        int p = g_state.pct_base;
        if (p < 5) p = 5;
        if (p > 95) p = 95;
        g_state.autotune_split = p;
      }
    }

    double pv = g_state.temp_cielo;
    if (g_state.sensor_mode == SensorMode::DUAL) {
      if (!g_state.tc_base_err && !g_state.tc_cielo_err) {
        pv = (g_state.temp_base + g_state.temp_cielo) * 0.5;
      } else if (!g_state.tc_cielo_err) {
        pv = g_state.temp_cielo;
      } else if (!g_state.tc_base_err) {
        pv = g_state.temp_base;
      }
    }

    at_input  = pv;
    at_output = 50.0;  // output iniziale 50%

    g_state.autotune_status = AutotuneStatus::RUNNING;
    g_state.autotune_cycles = 0;
    g_state.autotune_kp     = 0;
    g_state.autotune_ki     = 0;
    g_state.autotune_kd     = 0;
    xSemaphoreGive(g_mutex);
  }

  // Configura libreria autotune
  at_tuner.SetOutputStep(AUTOTUNE_OUTPUT_STEP);
  at_tuner.SetNoiseBand(AUTOTUNE_NOISE_BAND);
  at_tuner.SetLookbackSec(AUTOTUNE_LOOKBACK_S);
  at_tuner.SetControlType(1);   // 1 = PID (non solo PI)

  at_running     = true;
  at_start_ms    = millis();
  at_prev_cycles = 0;

  Serial.printf("[AUTOTUNE] Avviato — mode=%s  PV=%.1f°C  split=%d/%d\n",
    g_state.sensor_mode == SensorMode::SINGLE ? "SINGLE" : "DUAL",
    (float)at_input,
    g_state.autotune_split,
    100 - g_state.autotune_split);
}

// ================================================================
//  autotune_stop — interrompe e ripristina parametri precedenti
// ================================================================
void autotune_stop() {
  if (!at_running) return;
  at_running = false;
  at_tuner.Cancel();

  if (MUTEX_TAKE_MS(50)) {
    // Ripristina parametri originali
    g_state.kp_base  = saved_kp_base;
    g_state.ki_base  = saved_ki_base;
    g_state.kd_base  = saved_kd_base;
    g_state.kp_cielo = saved_kp_cielo;
    g_state.ki_cielo = saved_ki_cielo;
    g_state.kd_cielo = saved_kd_cielo;
    g_state.autotune_status = AutotuneStatus::ABORTED;
    g_state.base_enabled  = saved_base_enabled;
    g_state.cielo_enabled = saved_cielo_enabled;
    xSemaphoreGive(g_mutex);
  }

  RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
  RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);

  Serial.println("[AUTOTUNE] Interrotto — parametri originali ripristinati");
}

// ================================================================
//  autotune_is_running
// ================================================================
bool autotune_is_running() { return at_running; }

// ================================================================
//  autotune_run — chiamato ogni PID_SAMPLE_MS in Task_PID
//
//  Logica relay durante autotune:
//    output = 0..100% (gestito dalla libreria)
//    split applica: base = output * split/100
//                   cielo= output * (100-split)/100
//    Relay BASE e CIELO commutano con time-proportional
//    usando la stessa finestra PID normale (PID_WINDOW_MS)
// ================================================================
void autotune_run(float temp_pv, uint32_t now_ms) {
  if (!at_running) return;

  // Aggiorna input libreria (PV: Cielo in SINGLE, media in DUAL — calcolata in Task_PID)
  at_input = (double)temp_pv;

  // Chiama libreria — restituisce 0 se ancora in corso, 1 se finita
  int result = at_tuner.Runtime();

  // Aggiorna cicli completati per barra progresso
  // La libreria accumula picchi; ogni 2 picchi = 1 ciclo completo
  // Non c'è un getter diretto, quindi stimiamo dal tempo trascorso
  // e dal lookback (ogni ciclo ~= 2x lookback in condizioni tipiche)
  uint32_t elapsed_s = (now_ms - at_start_ms) / 1000;
  int estimated_cycles = (int)(elapsed_s / (AUTOTUNE_LOOKBACK_S * 2));
  if (estimated_cycles != at_prev_cycles) {
    at_prev_cycles = estimated_cycles;
    if (MUTEX_TAKE_MS(10)) {
      g_state.autotune_cycles = estimated_cycles;
      xSemaphoreGive(g_mutex);
    }
  }

  // Applica output relay con split configurata
  int split_base  = 50;
  int split_cielo = 50;
  if (MUTEX_TAKE_MS(10)) {
    split_base  = g_state.autotune_split;
    split_cielo = 100 - split_base;
    xSemaphoreGive(g_mutex);
  }

  double out_base  = at_output * split_base  / 100.0;
  double out_cielo = at_output * split_cielo / 100.0;

  // Durante autotune il PID non comanda i relay: la UI mostrerebbe 0%.
  // Pubbliciamo il duty richiesto dalla libreria (0–100% per zona) come pid_out_*.
  if (MUTEX_TAKE_MS(10)) {
    g_state.pid_out_base  = (float)out_base;
    g_state.pid_out_cielo = (float)out_cielo;
    xSemaphoreGive(g_mutex);
  }

  // Time-proportional relay
  static uint32_t win_base  = 0;
  static uint32_t win_cielo = 0;
  if (now_ms - win_base  >= PID_WINDOW_MS) win_base  = now_ms;
  if (now_ms - win_cielo >= PID_WINDOW_MS) win_cielo = now_ms;

  bool rb = (now_ms - win_base  < (uint32_t)(out_base  / 100.0 * PID_WINDOW_MS));
  bool rc = (now_ms - win_cielo < (uint32_t)(out_cielo / 100.0 * PID_WINDOW_MS));

  // Usa RELAY_WRITE (rispetta RELAY_*_INV), non digitalWrite grezzo
  RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  rb);
  RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, rc);
  g_state.relay_base  = rb;
  g_state.relay_cielo = rc;

  // Autotune terminato
  if (result != 0) {
    at_running = false;

    float kp = (float)at_tuner.GetKp();
    float ki = (float)at_tuner.GetKi();
    float kd = (float)at_tuner.GetKd();

    Serial.printf("[AUTOTUNE] COMPLETATO — Kp=%.3f Ki=%.4f Kd=%.3f\n", kp, ki, kd);

    if (MUTEX_TAKE_MS(50)) {
      // Applica stessi parametri a Base e Cielo
      g_state.kp_base  = kp;  g_state.ki_base  = ki;  g_state.kd_base  = kd;
      g_state.kp_cielo = kp;  g_state.ki_cielo = ki;  g_state.kd_cielo = kd;
      g_state.autotune_kp     = kp;
      g_state.autotune_ki     = ki;
      g_state.autotune_kd     = kd;
      g_state.autotune_status = AutotuneStatus::DONE;
      g_state.nvs_dirty       = true;   // salva su flash
      g_state.base_enabled    = saved_base_enabled;
      g_state.cielo_enabled   = saved_cielo_enabled;
      g_state.relay_base      = false;
      g_state.relay_cielo     = false;
      xSemaphoreGive(g_mutex);
    }

    RELAY_WRITE(RELAY_BASE,  RELAY_BASE_INV,  false);
    RELAY_WRITE(RELAY_CIELO, RELAY_CIELO_INV, false);
    s_just_completed = true;
  }
}
