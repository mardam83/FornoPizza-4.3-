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
static uint32_t at_start_ms       = 0;
static int      at_prev_cycles    = 0;

// ================================================================
//  autotune_start
// ================================================================
void autotune_start() {
  if (at_running) return;

  // Salva parametri correnti per eventuale ripristino
  if (MUTEX_TAKE_MS(50)) {
    saved_kp_base  = g_state.kp_base;
    saved_ki_base  = g_state.ki_base;
    saved_kd_base  = g_state.kd_base;
    saved_kp_cielo = g_state.kp_cielo;
    saved_ki_cielo = g_state.ki_cielo;
    saved_kd_cielo = g_state.kd_cielo;

    // Imposta setpoint autotune leggermente sotto quello corrente
    // per partire con il forno in salita controllata
    at_input  = (double)g_state.temp_cielo;
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

  Serial.printf("[AUTOTUNE] Avviato — setpoint=%.1f split=%d/%d\n",
    g_state.set_cielo,
    g_state.autotune_split,
    100 - g_state.autotune_split);
}

// ================================================================
//  autotune_stop — interrompe e ripristina parametri precedenti
// ================================================================
void autotune_stop() {
  if (!at_running) return;
  at_running = false;

  if (MUTEX_TAKE_MS(50)) {
    // Ripristina parametri originali
    g_state.kp_base  = saved_kp_base;
    g_state.ki_base  = saved_ki_base;
    g_state.kd_base  = saved_kd_base;
    g_state.kp_cielo = saved_kp_cielo;
    g_state.ki_cielo = saved_ki_cielo;
    g_state.kd_cielo = saved_kd_cielo;
    g_state.autotune_status = AutotuneStatus::ABORTED;
    // Spegni relay
    g_state.base_enabled  = false;
    g_state.cielo_enabled = false;
    xSemaphoreGive(g_mutex);
  }

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
void autotune_run(float temp_cielo, uint32_t now_ms) {
  if (!at_running) return;

  // Aggiorna input libreria
  at_input = (double)temp_cielo;

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
  int split_base  = g_state.autotune_split;
  int split_cielo = 100 - split_base;

  double out_base  = at_output * split_base  / 100.0;
  double out_cielo = at_output * split_cielo / 100.0;

  // Time-proportional relay
  static uint32_t win_base  = 0;
  static uint32_t win_cielo = 0;
  if (now_ms - win_base  >= PID_WINDOW_MS) win_base  = now_ms;
  if (now_ms - win_cielo >= PID_WINDOW_MS) win_cielo = now_ms;

  bool rb = (now_ms - win_base  < (uint32_t)(out_base  / 100.0 * PID_WINDOW_MS));
  bool rc = (now_ms - win_cielo < (uint32_t)(out_cielo / 100.0 * PID_WINDOW_MS));

  if (rb != g_state.relay_base)  {
    g_state.relay_base  = rb;
    digitalWrite(RELAY_BASE,  rb ? HIGH : LOW);
  }
  if (rc != g_state.relay_cielo) {
    g_state.relay_cielo = rc;
    digitalWrite(RELAY_CIELO, rc ? HIGH : LOW);
  }

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
      g_state.base_enabled    = false;  // spegni dopo autotune
      g_state.cielo_enabled   = false;
      xSemaphoreGive(g_mutex);
    }

    // Spegni relay
    digitalWrite(RELAY_BASE,  LOW);
    digitalWrite(RELAY_CIELO, LOW);
  }
}
