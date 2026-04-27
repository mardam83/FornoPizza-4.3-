/**
 * simulator.cpp — Forno Pizza S3 — Simulatore Hardware + Test Sequencer v3.0
 */

#include "simulator.h"
#include "ui.h"        // g_state, AppState, AutotuneStatus
#include "hardware.h"  // MUTEX_TAKE_MS, MUTEX_GIVE
#include "autotune.h"  // autotune_start(), autotune_is_running()
#include <Arduino.h>

// Override finestra RUNAWAY_DOWN (solo test simulatore; letto da Task_PID in SIM)
uint32_t g_runaway_down_ms_override = 0;

// ================================================================
//  STATO GLOBALE
// ================================================================
SimulatorState g_sim = {};

// ================================================================
//  Duty cycle tracker (finestra scorrevole 30s @ 500ms = 60 campioni)
// ================================================================
#define DUTY_HIST_SIZE  60
static float    s_duty_hist[DUTY_HIST_SIZE] = {};
static int      s_duty_idx = 0;

static float calc_avg_duty() {
    float sum = 0.0f;
    for (int i = 0; i < DUTY_HIST_SIZE; i++) sum += s_duty_hist[i];
    return sum / DUTY_HIST_SIZE;
}

// ================================================================
//  Log helpers
// ================================================================
static void log_separator(char c = '-') {
    for (int i = 0; i < 60; i++) Serial.print(c);
    Serial.println();
}

static void log_phase_header(const char* title) {
    Serial.println();
    log_separator('=');
    Serial.printf("[TEST] %s\n", title);
    log_separator('=');
}

static void log_pass(const char* what) {
    Serial.printf("[TEST] ✓ PASS: %s\n", what);
}

static void log_fail(const char* what) {
    Serial.printf("[TEST] ✗ FAIL: %s\n", what);
}

// ================================================================
//  simulator_init
// ================================================================
void simulator_init() {
    memset(&g_sim, 0, sizeof(g_sim));
    g_sim.temp_c        = SIM_T_START;
    g_sim.phase         = SimTestPhase::WAIT_START;
    g_sim.phase_start_ms = millis();
    memset(s_duty_hist, 0, sizeof(s_duty_hist));
    s_duty_idx = 0;

    log_separator('=');
    Serial.println("[SIM] Simulatore termico v3.0 (≈35×35×10 cm, ~2200 W)");
    Serial.printf ("[SIM] Modello: P=%.0fW  k=%.1fW/C  M=%.0fJ/C  scale=%.1fx\n",
                   SIM_POWER_W, SIM_K_LOSS, SIM_THERMAL_MASS, SIM_TIME_SCALE);
    Serial.printf ("[SIM] T_eq teorica (P=k·ΔT): ≈ %.0f°C @ pieno carico\n",
                   SIM_T_AMBIENT + SIM_POWER_W / SIM_K_LOSS);
    Serial.printf ("[SIM] T_start=%.1f°C  T_amb=%.1f°C\n", SIM_T_START, SIM_T_AMBIENT);
    Serial.println("[SIM] MAX6675 mock attivo");
    log_separator();
    Serial.println("[SIM] SEQUENZA TEST AUTOMATICA:");
    Serial.println("[SIM]   1 PID + relay/ventola  2 OVERTEMP  3 TC_ERROR");
    Serial.println("[SIM]   4 RUNAWAY_UP  5 RUNAWAY_DOWN  6 AUTOTUNE  7 FINALE  8 REPORT");
    log_separator();
    Serial.printf("[SIM] Auto-avvio in %.1f secondi, oppure premi BASE ON sul display\n",
                  SIM_AUTOSTART_MS / 1000.0f);
    log_separator('=');
    Serial.println();
}

// ================================================================
//  simulator_reset_thermal — riporta forno a temperatura ambiente
// ================================================================
void simulator_user_turned_heat_off(void) {
    g_sim.user_abort_autostart = true;
}

void simulator_reset_thermal() {
    g_sim.temp_c          = SIM_T_START;
    g_sim.relay_on        = false;
    g_sim.force_tc_error  = false;
    g_sim.force_overtemp  = false;
    g_sim.tc_error_count  = 0;
    g_sim.ghost_heat      = false;
    g_sim.ghost_power_w   = 0.0f;
    g_sim.rwd_inject      = false;
    g_sim.rwd_subphase    = 0;
    g_sim.thermal_reset_seq++;
    g_runaway_down_ms_override = 0;
    memset(s_duty_hist, 0, sizeof(s_duty_hist));
    s_duty_idx = 0;
    g_sim.duty_avg = 0.0f;

    // Resetta stato applicazione via mutex
    if (MUTEX_TAKE_MS(50)) {
        g_state.temp_base       = SIM_T_START;
        g_state.temp_cielo      = SIM_T_START;
        g_state.relay_base      = false;
        g_state.relay_cielo     = false;
        g_state.fan_on          = false;
        g_state.safety_shutdown = false;
        g_state.safety_reason   = SafetyReason::NONE;
        g_state.tc_base_err     = false;
        g_state.tc_cielo_err    = false;
        MUTEX_GIVE();
    }
    // Resetta anche il flag globale di emergency shutdown
    extern volatile bool g_emergency_shutdown;
    g_emergency_shutdown = false;

    Serial.printf("[SIM] Reset termico → T=%.1f°C\n", SIM_T_START);
}

// ================================================================
//  simulator_set_relay
// ================================================================
void simulator_set_relay(bool base_on, bool cielo_on) {
    g_sim.relay_on = base_on || cielo_on;
    s_duty_hist[s_duty_idx] = g_sim.relay_on ? 1.0f : 0.0f;
    s_duty_idx = (s_duty_idx + 1) % DUTY_HIST_SIZE;
    g_sim.duty_avg = calc_avg_duty();
}

// ================================================================
//  simulator_tick — aggiorna modello termico
// ================================================================
void simulator_tick(uint32_t dt_ms) {
    float dt_s = (float)dt_ms / 1000.0f * SIM_TIME_SCALE;

    float p_in   = g_sim.relay_on ? SIM_POWER_W : 0.0f;
    // Calore “fantasma” con relay logicamente spenti (test RUNAWAY_UP)
    if (g_sim.ghost_heat && !g_sim.relay_on) {
        p_in += g_sim.ghost_power_w;
    }
    float p_loss = SIM_K_LOSS * (g_sim.temp_c - SIM_T_AMBIENT);
    // Raffreddamento extra durante test RUNAWAY_DOWN (relay ancora ON)
    if (g_sim.rwd_inject && g_sim.relay_on) {
        p_loss += 25.0f * (g_sim.temp_c - SIM_T_AMBIENT);
    }
    float dT     = dt_s * (p_in - p_loss) / SIM_THERMAL_MASS;

    g_sim.temp_c += dT;
    if (g_sim.temp_c < SIM_T_AMBIENT) g_sim.temp_c = SIM_T_AMBIENT;
    if (g_sim.temp_c > 600.0f)        g_sim.temp_c = 600.0f;

    g_sim.power_in       = p_in;
    g_sim.heat_loss      = p_loss;
    g_sim.ticks++;
    g_sim.time_elapsed_s += dt_s;
}

// ================================================================
//  simulator_print_status
// ================================================================
void simulator_print_status() {
    const char* phase_names[] = {
        "WAIT", "PID", "OVTMP", "TC_ERR", "RW_UP", "RW_DN",
        "ATUNE", "FINAL", "REPORT", "DONE"
    };
    int ph = (int)g_sim.phase;
    Serial.printf("[SIM t=%6.0fs] T=%6.1f°C  relay=%c  duty=%3.0f%%  "
                  "P_in=%4.0fW  P_loss=%4.0fW  fase=%s\n",
                  g_sim.time_elapsed_s,
                  g_sim.temp_c,
                  g_sim.relay_on ? 'O' : '.',
                  g_sim.duty_avg * 100.0f,
                  g_sim.power_in,
                  g_sim.heat_loss,
                  (ph >= 0 && ph <= 9) ? phase_names[ph] : "?");
}

// ================================================================
//  simulator_notify_shutdown — chiamato da emergency_shutdown()
// ================================================================
void simulator_notify_shutdown(int reason_int) {
    SafetyReason r = (SafetyReason)reason_int;
    switch (g_sim.phase) {
        case SimTestPhase::OVERTEMP:
            if (r == SafetyReason::OVERTEMP) {
                g_sim.test_overtemp_ok = true;
                log_pass("OVERTEMP safety triggered correttamente");
            } else {
                log_fail("OVERTEMP: shutdown con reason sbagliata");
            }
            break;
        case SimTestPhase::TC_ERROR:
            if (r == SafetyReason::TC_ERROR) {
                g_sim.test_tc_error_ok = true;
                log_pass("TC_ERROR safety triggered correttamente");
            } else {
                log_fail("TC_ERROR: shutdown con reason sbagliata");
            }
            break;
        case SimTestPhase::RUNAWAY_UP:
            if (r == SafetyReason::RUNAWAY_UP) {
                g_sim.test_runaway_up_ok = true;
                log_pass("RUNAWAY_UP safety triggered correttamente");
            } else {
                log_fail("RUNAWAY_UP: reason errata");
            }
            break;
        case SimTestPhase::RUNAWAY_DOWN:
            if (r == SafetyReason::RUNAWAY_DOWN) {
                g_sim.test_runaway_down_ok = true;
                log_pass("RUNAWAY_DOWN safety triggered correttamente");
            } else {
                log_fail("RUNAWAY_DOWN: reason errata");
            }
            break;
        default:
            Serial.printf("[SIM] WARN: shutdown inatteso in fase %d reason=%d\n",
                          (int)g_sim.phase, reason_int);
            break;
    }
}

// ================================================================
//  simulator_test_tick — macchina a stati del test
//  Chiamata ogni ciclo PID (dopo simulator_tick)
// ================================================================
void simulator_test_tick(uint32_t now_ms) {
    // Log periodico ogni 2s (indipendente dalla fase)
    static uint32_t s_last_log_ms = 0;
    if (now_ms - s_last_log_ms >= 2000) {
        s_last_log_ms = now_ms;
        simulator_print_status();
    }

    uint32_t phase_elapsed = now_ms - g_sim.phase_start_ms;

    switch (g_sim.phase) {

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::WAIT_START:
    {
        // Auto-avvio dopo SIM_AUTOSTART_MS se base non è già attiva
        /* Timer 8s: non forzare se l'utente ha spento (altrimenti riaccende i relay). */
        bool should_start = g_state.base_enabled
                            || (phase_elapsed >= SIM_AUTOSTART_MS && !g_sim.user_abort_autostart);
        if (should_start) {
            log_phase_header("FASE 1 — RISCALDAMENTO PID");
            Serial.printf("[TEST] Setpoint: %.0f°C  Kp=%.2f Ki=%.3f Kd=%.2f\n",
                          g_state.set_base, g_state.kp_base,
                          g_state.ki_base, g_state.kd_base);
            Serial.printf("[TEST] SIM_TIME_SCALE=%.1fx  → 10 min reali ≈ %.1f min simulati\n",
                          SIM_TIME_SCALE,
                          10.0f * SIM_TIME_SCALE);

            // Attiva riscaldamento
            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = true;
                g_state.cielo_enabled = true;
                MUTEX_GIVE();
            }
            g_sim.phase           = SimTestPhase::PID_WARMUP;
            g_sim.phase_start_ms  = now_ms;
            g_sim.pid_settle_time_s = 0.0f;
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::PID_WARMUP:
    {
        float err = fabsf(g_sim.temp_c - (float)g_state.set_base);
        float elapsed_s = phase_elapsed / 1000.0f;

        // Log dettagliato ogni 10s
        static uint32_t s_last_pid_log = 0;
        if (now_ms - s_last_pid_log >= 10000) {
            s_last_pid_log = now_ms;
            Serial.printf("[TEST-PID] t=%.0fs  T=%.1f°C  SP=%.0f°C  err=%.1f°C  "
                          "out=%.1f%%  duty=%.0f%%  relay=%c\n",
                          elapsed_s, g_sim.temp_c, g_state.set_base, err,
                          g_state.pid_out_base, g_sim.duty_avg * 100.0f,
                          g_sim.relay_on ? 'O' : '.');
        }

        // Copertura relay (duty medio) e ventola quando T > soglia
        if (elapsed_s > 35.0f && g_sim.duty_avg > 0.03f) {
            g_sim.test_relay_duty_ok = true;
        }
        if (g_sim.temp_c > FAN_OFF_TEMP + 5.0f) {
            if (MUTEX_TAKE_MS(20)) {
                if (g_state.fan_on) g_sim.test_fan_ok = true;
                MUTEX_GIVE();
            }
        }

        // Considera stabilizzato se |err| < 5°C per almeno 20s simulati
        static float s_stable_since_s = -1.0f;
        if (err < 5.0f) {
            if (s_stable_since_s < 0.0f) s_stable_since_s = g_sim.time_elapsed_s;
            float stable_for = g_sim.time_elapsed_s - s_stable_since_s;
            if (stable_for >= 20.0f) {
                bool aux_ok = g_sim.test_relay_duty_ok && g_sim.test_fan_ok;
                if (!aux_ok) {
                    log_fail("PID: duty relay o ventola non coerenti col modello");
                }
                g_sim.test_pid_ok       = aux_ok;
                g_sim.pid_settle_time_s = elapsed_s;
                if (aux_ok) log_pass("PID stabilizzato a setpoint (relay+fan OK)");
                Serial.printf("[TEST] T=%.1f°C  err=%.1f°C  tempo_stabiliz=%.0fs  "
                              "duty_avg=%.0f%%  fan=%s\n",
                              g_sim.temp_c, err, elapsed_s,
                              g_sim.duty_avg * 100.0f,
                              g_sim.test_fan_ok ? "OK" : "NO");
                s_stable_since_s = -1.0f;

                // Avanza a FASE 2
                log_phase_header("FASE 2 — TEST OVERTEMP SAFETY");
                Serial.printf("[TEST] Forzo temperatura a 495°C (TEMP_MAX_SAFE=%.0f°C)\n",
                              TEMP_MAX_SAFE);
                Serial.println("[TEST] Atteso: emergency_shutdown(OVERTEMP)");
                g_sim.force_overtemp = true;
                g_sim.phase          = SimTestPhase::OVERTEMP;
                g_sim.phase_start_ms = now_ms;
            }
        } else {
            s_stable_since_s = -1.0f;
        }

        // Timeout sicurezza: max 15 min simulati (=3.75 min reali a 4x)
        if (elapsed_s > 900.0f && !g_sim.test_pid_ok) {
            log_fail("PID non stabilizzato entro timeout (900s simulati)");
            g_sim.test_pid_ok = false;
            // Passa comunque al test successivo
            g_sim.force_overtemp = true;
            g_sim.phase          = SimTestPhase::OVERTEMP;
            g_sim.phase_start_ms = now_ms;
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::OVERTEMP:
    {
        // Attende che il safety system abbia risposto (entro 3s reali)
        if (phase_elapsed > 3000) {
            if (!g_sim.test_overtemp_ok) {
                log_fail("OVERTEMP: safety non scattato entro 3s");
            }
            // Reset e avanza a FASE 3
            g_sim.force_overtemp = false;
            simulator_reset_thermal();

            log_phase_header("FASE 3 — TEST TC_ERROR SAFETY");
            Serial.println("[TEST] Forzo readCelsius() → NAN per 5 letture");
            Serial.println("[TEST] In SINGLE mode atteso: warn open-loop (NO shutdown)");
            Serial.println("[TEST] (Per testare shutdown TC_ERROR: cambiare in DUAL mode)");

            g_sim.force_tc_error = true;
            g_sim.tc_error_count = 5;
            g_sim.phase          = SimTestPhase::TC_ERROR;
            g_sim.phase_start_ms = now_ms;

            // Riattiva resistenze per il test
            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = true;
                g_state.cielo_enabled = true;
                MUTEX_GIVE();
            }
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::TC_ERROR:
    {
        // Attende fine iniezione errori TC
        if (!g_sim.force_tc_error && phase_elapsed > 3000) {
            // In SINGLE mode: nessuno shutdown, solo warning → PASS
            // In DUAL mode: shutdown TC_ERROR → verificato da notify_shutdown
            bool is_single = (g_state.sensor_mode == SensorMode::SINGLE);
            if (is_single) {
                // Verifica che NON ci sia stato shutdown (SINGLE = aperto, non shutdown)
                if (!g_state.safety_shutdown) {
                    g_sim.test_tc_error_ok = true;
                    log_pass("TC_ERROR in SINGLE mode: warn senza shutdown (corretto)");
                } else {
                    log_fail("TC_ERROR in SINGLE mode: shutdown inatteso");
                }
            } else {
                // DUAL mode: deve essere scattato
                if (g_sim.test_tc_error_ok) {
                    log_pass("TC_ERROR in DUAL mode: shutdown corretto");
                } else {
                    log_fail("TC_ERROR in DUAL mode: shutdown non avvenuto");
                }
            }

            // Reset e avanza a FASE 4 — RUNAWAY_UP
            simulator_reset_thermal();

            log_phase_header("FASE 4 — TEST RUNAWAY_UP");
            Serial.println("[TEST] Calore fantasma con relay logicamente OFF (sim. SSR incollato)");
            Serial.printf("[TEST] Atteso: emergency_shutdown(RUNAWAY_UP) entro ~%lu ms\n",
                          (unsigned long)RUNAWAY_RISE_MS);
            g_sim.ghost_heat    = true;
            g_sim.ghost_power_w = SIM_POWER_W;
            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = false;
                g_state.cielo_enabled = false;
                MUTEX_GIVE();
            }

            g_sim.phase          = SimTestPhase::RUNAWAY_UP;
            g_sim.phase_start_ms = now_ms;
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::RUNAWAY_UP:
    {
        if (g_sim.test_runaway_up_ok) {
            g_sim.ghost_heat = false;
            simulator_reset_thermal();
            g_runaway_down_ms_override = SIM_RUNAWAY_DOWN_MS_TEST;

            log_phase_header("FASE 5 — TEST RUNAWAY_DOWN");
            Serial.println("[TEST] Riscaldamento poi calo forzato (sim. elemento rotto)");
            Serial.printf("[TEST] Finestra RUNAWAY_DOWN sim: %lu ms (HW usa %lu ms)\n",
                          (unsigned long)SIM_RUNAWAY_DOWN_MS_TEST,
                          (unsigned long)RUNAWAY_DOWN_MS);
            g_sim.rwd_subphase = 0;
            g_sim.phase          = SimTestPhase::RUNAWAY_DOWN;
            g_sim.phase_start_ms = now_ms;
            break;
        }
        if (phase_elapsed > 45000) {
            if (!g_sim.test_runaway_up_ok) {
                log_fail("RUNAWAY_UP: timeout");
            }
            g_sim.ghost_heat = false;
            simulator_reset_thermal();
            g_runaway_down_ms_override = SIM_RUNAWAY_DOWN_MS_TEST;
            g_sim.rwd_subphase = 0;
            g_sim.phase          = SimTestPhase::RUNAWAY_DOWN;
            g_sim.phase_start_ms = now_ms;
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::RUNAWAY_DOWN:
    {
        g_runaway_down_ms_override = SIM_RUNAWAY_DOWN_MS_TEST;

        if (g_sim.test_runaway_down_ok) {
            g_sim.rwd_inject = false;
            g_runaway_down_ms_override = 0;
            simulator_reset_thermal();

            log_phase_header("FASE 6 — TEST AUTOTUNE PID");
            Serial.printf("[TEST] Setpoint autotune: %.0f°C\n", g_state.set_base);
            Serial.println("[TEST] Relay autotune → modello termico (fix Task_PID)");
            Serial.printf("[TEST] Parametri attuali: Kp=%.2f Ki=%.3f Kd=%.2f\n",
                          g_state.kp_base, g_state.ki_base, g_state.kd_base);

            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = true;
                g_state.cielo_enabled = true;
                MUTEX_GIVE();
            }
            autotune_apply_default_split();
            autotune_start();

            g_sim.phase          = SimTestPhase::AUTOTUNE;
            g_sim.phase_start_ms = now_ms;
            break;
        }

        if (g_sim.rwd_subphase == 0) {
            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = true;
                g_state.cielo_enabled = true;
                MUTEX_GIVE();
            }
            if (g_sim.temp_c >= 300.0f) {
                g_sim.rwd_subphase = 1;
                Serial.println("[TEST] Picco termico raggiunto → iniezione calo");
            }
        } else {
            g_sim.rwd_inject = true;
        }

        if (phase_elapsed > 150000) {
            if (!g_sim.test_runaway_down_ok) {
                log_fail("RUNAWAY_DOWN: timeout");
            }
            g_sim.rwd_inject = false;
            g_runaway_down_ms_override = 0;
            simulator_reset_thermal();

            log_phase_header("FASE 6 — TEST AUTOTUNE PID (dopo RUNAWAY_DOWN timeout)");
            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = true;
                g_state.cielo_enabled = true;
                MUTEX_GIVE();
            }
            autotune_apply_default_split();
            autotune_start();
            g_sim.phase          = SimTestPhase::AUTOTUNE;
            g_sim.phase_start_ms = now_ms;
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::AUTOTUNE:
    {
        float elapsed_s = phase_elapsed / 1000.0f;

        // Log stato autotune ogni 15s
        static uint32_t s_last_at_log = 0;
        if (now_ms - s_last_at_log >= 15000) {
            s_last_at_log = now_ms;
            Serial.printf("[TEST-AT] t=%.0fs  T=%.1f°C  running=%d  cycles=%d\n",
                          elapsed_s, g_sim.temp_c,
                          autotune_is_running() ? 1 : 0,
                          g_state.autotune_cycles);
        }

        // Autotune completato
        if (g_state.autotune_status == AutotuneStatus::DONE && !autotune_is_running()) {
            g_sim.autotune_kp_result = g_state.kp_base;
            g_sim.autotune_ki_result = g_state.ki_base;
            g_sim.autotune_kd_result = g_state.kd_base;

            bool kp_ok = (g_sim.autotune_kp_result > 0.1f && g_sim.autotune_kp_result < 50.0f);
            bool ki_ok = (g_sim.autotune_ki_result >= 0.0f && g_sim.autotune_ki_result < 5.0f);
            bool kd_ok = (g_sim.autotune_kd_result >= 0.0f && g_sim.autotune_kd_result < 100.0f);
            g_sim.test_autotune_ok = kp_ok && ki_ok;

            if (g_sim.test_autotune_ok) {
                log_pass("Autotune completato con parametri validi");
            } else {
                log_fail("Autotune: parametri fuori range");
            }
            Serial.printf("[TEST] Kp=%.3f  Ki=%.4f  Kd=%.3f  "
                          "(range ok: Kp=%s Ki=%s Kd=%s)\n",
                          g_sim.autotune_kp_result,
                          g_sim.autotune_ki_result,
                          g_sim.autotune_kd_result,
                          kp_ok ? "SI" : "NO",
                          ki_ok ? "SI" : "NO",
                          kd_ok ? "SI" : "NO");
            Serial.printf("[TEST] Durata autotune: %.0fs simulati\n", elapsed_s);

            // Avanza a FASE 7
            log_phase_header("FASE 7 — RISCALDAMENTO FINALE (parametri autotune)");
            Serial.printf("[TEST] Kp=%.3f Ki=%.4f Kd=%.3f\n",
                          g_state.kp_base, g_state.ki_base, g_state.kd_base);

            // Riattiva resistenze (autotune le ha spente)
            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = true;
                g_state.cielo_enabled = true;
                MUTEX_GIVE();
            }
            g_sim.phase          = SimTestPhase::FINAL_WARMUP;
            g_sim.phase_start_ms = now_ms;
        }

        // Timeout autotune: 20 minuti simulati
        if (elapsed_s > 1200.0f && autotune_is_running()) {
            Serial.println("[TEST] WARN: autotune timeout — forzo stop");
            autotune_stop();
            g_sim.test_autotune_ok = false;
            log_fail("Autotune non completato entro timeout");
            g_sim.phase          = SimTestPhase::FINAL_WARMUP;
            g_sim.phase_start_ms = now_ms;
            if (MUTEX_TAKE_MS(50)) {
                g_state.base_enabled  = true;
                g_state.cielo_enabled = true;
                MUTEX_GIVE();
            }
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::FINAL_WARMUP:
    {
        float err = fabsf(g_sim.temp_c - (float)g_state.set_base);
        float elapsed_s = phase_elapsed / 1000.0f;

        static uint32_t s_last_fw_log = 0;
        if (now_ms - s_last_fw_log >= 10000) {
            s_last_fw_log = now_ms;
            Serial.printf("[TEST-FIN] t=%.0fs  T=%.1f°C  err=%.1f°C  out=%.1f%%\n",
                          elapsed_s, g_sim.temp_c, err, g_state.pid_out_base);
        }

        static float s_stable_since_s = -1.0f;
        if (err < 5.0f) {
            if (s_stable_since_s < 0.0f) s_stable_since_s = g_sim.time_elapsed_s;
            float stable_for = g_sim.time_elapsed_s - s_stable_since_s;
            if (stable_for >= 20.0f) {
                g_sim.test_final_ok = true;
                log_pass("Riscaldamento finale stabilizzato");
                Serial.printf("[TEST] T=%.1f°C  err=%.1f°C  tempo=%.0fs\n",
                              g_sim.temp_c, err, elapsed_s);
                s_stable_since_s = -1.0f;
                g_sim.phase          = SimTestPhase::REPORT;
                g_sim.phase_start_ms = now_ms;
            }
        } else {
            s_stable_since_s = -1.0f;
        }

        if (elapsed_s > 900.0f && !g_sim.test_final_ok) {
            log_fail("Riscaldamento finale non stabilizzato entro timeout");
            g_sim.phase          = SimTestPhase::REPORT;
            g_sim.phase_start_ms = now_ms;
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::REPORT:
    {
        // Stampa una sola volta
        static bool s_reported = false;
        if (!s_reported) {
            s_reported = true;

            // Conta risultati
            g_sim.tests_total  = 7;
            g_sim.tests_passed = (g_sim.test_pid_ok          ? 1 : 0)
                               + (g_sim.test_overtemp_ok   ? 1 : 0)
                               + (g_sim.test_tc_error_ok   ? 1 : 0)
                               + (g_sim.test_runaway_up_ok ? 1 : 0)
                               + (g_sim.test_runaway_down_ok ? 1 : 0)
                               + (g_sim.test_autotune_ok   ? 1 : 0)
                               + (g_sim.test_final_ok      ? 1 : 0);

            Serial.println();
            log_separator('*');
            Serial.println("[TEST] ===== REPORT FINALE =====");
            log_separator('*');
            Serial.printf("[TEST] Tempo totale simulato: %.0f s (%.1f min)\n",
                          g_sim.time_elapsed_s, g_sim.time_elapsed_s / 60.0f);
            Serial.println("[TEST]");
            Serial.printf("[TEST]  [%s] FASE 1 — PID + relay/ventola (settle=%.0fs)\n",
                          g_sim.test_pid_ok ? "PASS" : "FAIL",
                          g_sim.pid_settle_time_s);
            Serial.printf("[TEST]  [%s] FASE 2 — OVERTEMP safety\n",
                          g_sim.test_overtemp_ok ? "PASS" : "FAIL");
            Serial.printf("[TEST]  [%s] FASE 3 — TC_ERROR safety\n",
                          g_sim.test_tc_error_ok ? "PASS" : "FAIL");
            Serial.printf("[TEST]  [%s] FASE 4 — RUNAWAY_UP\n",
                          g_sim.test_runaway_up_ok ? "PASS" : "FAIL");
            Serial.printf("[TEST]  [%s] FASE 5 — RUNAWAY_DOWN\n",
                          g_sim.test_runaway_down_ok ? "PASS" : "FAIL");
            Serial.printf("[TEST]  [%s] FASE 6 — Autotune PID"
                          "  (Kp=%.3f Ki=%.4f Kd=%.3f)\n",
                          g_sim.test_autotune_ok ? "PASS" : "FAIL",
                          g_sim.autotune_kp_result,
                          g_sim.autotune_ki_result,
                          g_sim.autotune_kd_result);
            Serial.printf("[TEST]  [%s] FASE 7 — Riscaldamento finale\n",
                          g_sim.test_final_ok ? "PASS" : "FAIL");
            Serial.println("[TEST]");
            log_separator('-');
            Serial.printf("[TEST] RISULTATO: %d/%d TEST PASSATI  %s\n",
                          g_sim.tests_passed, g_sim.tests_total,
                          g_sim.tests_passed == g_sim.tests_total ? "✓ TUTTO OK" : "✗ ERRORI");
            log_separator('*');
            Serial.println();
            Serial.println("[SIM] Sequenza test completata.");
            Serial.println("[SIM] Il forno rimane attivo per test manuali dal display.");
            Serial.println();

            g_sim.phase = SimTestPhase::DONE;
        }
        break;
    }

    // ──────────────────────────────────────────────────────────────
    case SimTestPhase::DONE:
        // Niente da fare — il forno continua a funzionare normalmente
        break;

    default:
        break;
    }
}
