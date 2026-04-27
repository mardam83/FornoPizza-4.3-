/**
 * simulator.h — Forno Pizza S3 — Simulatore Hardware + Test Sequencer v3.0
 * ================================================================
 * MODELLO TERMICO (first-order, Newton cooling):
 *   dT/dt = SIM_TIME_SCALE * (P_in - k_loss*(T-T_amb)) / thermal_mass
 *
 * Riferimento fisico (forno pizza elettrico compatto):
 *   Camera interna indicativa ~35 × 35 × 10 cm (volume ~12 L).
 *   Resistenze combinate ~2200 W nominali → P_in max ≈ SIM_POWER_W.
 *   Dispersioni verso ambiente modellate con k_loss [W/°C]: a regime
 *     T_eq ≈ T_amb + P_in/k_loss  (es. 2200/6 ≈ 367 °C sopra ambiente).
 *   Capacità termica efficace ~SIM_THERMAL_MASS [J/°C] (pietra, telaio, aria):
 *     costante di tempo τ ≈ SIM_THERMAL_MASS/k_loss (~3–4 min in tempo reale a scala 1x).
 *
 *   SIM_TIME_SCALE = 4.0   → accelerazione tempo simulato (test più rapidi).
 *
 * SEQUENZA TEST AUTOMATICA (simulator_test_tick):
 *   FASE 1 — PID + verifica duty relay / ventola (T > soglia ventola)
 *   FASE 2 — OVERTEMP
 *   FASE 3 — TC_ERROR (SINGLE: nessuno shutdown)
 *   FASE 4 — RUNAWAY_UP (calore fantasma a relay OFF)
 *   FASE 5 — RUNAWAY_DOWN (calo forzato con richiesta calore; finestra MS ridotta in SIM)
 *   FASE 6 — AUTOTUNE (relay visti dal modello termico dopo fix Task_PID)
 *   FASE 7 — Riscaldamento finale
 *   FASE 8 — Report
 * ================================================================
 */

#pragma once
#include <Arduino.h>
#include <math.h>

// ================================================================
//  PARAMETRI MODELLO TERMICO (allineati a ~2200 W, camera ~35×35×10 cm)
// ================================================================
#define SIM_POWER_W         2200.0f
#define SIM_K_LOSS            6.0f
#define SIM_THERMAL_MASS    1100.0f
#define SIM_T_AMBIENT         20.0f
#define SIM_T_START           22.0f
#define SIM_NOISE_DEG         0.25f
#define SIM_TIME_SCALE         4.0f

#define SIM_AUTOSTART_MS      8000

// Finestra RUNAWAY_DOWN nel solo test simulatore (ms reali; 0 = usa firmware default)
#define SIM_RUNAWAY_DOWN_MS_TEST  20000UL

// ================================================================
//  FASI TEST
// ================================================================
enum class SimTestPhase {
    WAIT_START   = 0,
    PID_WARMUP   = 1,
    OVERTEMP     = 2,
    TC_ERROR     = 3,
    RUNAWAY_UP   = 4,
    RUNAWAY_DOWN = 5,
    AUTOTUNE     = 6,
    FINAL_WARMUP = 7,
    REPORT       = 8,
    DONE         = 9
};

// ================================================================
//  STATO SIMULATORE
// ================================================================
struct SimulatorState {
    float    temp_c;
    bool     relay_on;
    float    power_in;
    float    heat_loss;
    uint32_t ticks;
    float    time_elapsed_s;

    float    duty_avg;

    SimTestPhase phase;
    uint32_t     phase_start_ms;
    bool         force_tc_error;
    bool         force_overtemp;
    int          tc_error_count;

    // Test RUNAWAY_UP: potenza aggiunta con relay logicamente spenti (simula SSR incollato)
    bool         ghost_heat;
    float        ghost_power_w;
    // Test RUNAWAY_DOWN: inietta raffreddamento extra mentre relay ON
    bool         rwd_inject;
    uint8_t      rwd_subphase;   // 0=riscaldamento 1=iniezione calo (RUNAWAY_DOWN)

    uint32_t     thermal_reset_seq;

    bool    test_pid_ok;
    bool    test_relay_duty_ok;
    bool    test_fan_ok;
    bool    test_overtemp_ok;
    bool    test_tc_error_ok;
    bool    test_runaway_up_ok;
    bool    test_runaway_down_ok;
    bool    test_autotune_ok;
    bool    test_final_ok;
    /** Se true: non avviare la sequenza test col timer (utente ha spento manualmente). */
    bool    user_abort_autostart;
    float   autotune_kp_result;
    float   autotune_ki_result;
    float   autotune_kd_result;
    float   pid_settle_time_s;
    int     tests_passed;
    int     tests_total;
};
extern SimulatorState g_sim;

// ================================================================
//  SimulatedMAX6675
// ================================================================
class SimulatedMAX6675 {
public:
    SimulatedMAX6675(int sck, int cs, int miso) {
        (void)sck; (void)cs; (void)miso;
    }

    float readCelsius() {
        if (g_sim.force_tc_error) {
            g_sim.tc_error_count--;
            if (g_sim.tc_error_count <= 0) {
                g_sim.force_tc_error = false;
            }
            return NAN;
        }
        if (g_sim.force_overtemp) {
            return 495.0f;
        }
        float noise = 0.0f;
        if (SIM_NOISE_DEG > 0.0f) {
            float u = ((float)random(1, 10000)) / 10000.0f;
            float v = ((float)random(1, 10000)) / 10000.0f;
            noise = SIM_NOISE_DEG * sqrtf(-2.0f * logf(u)) * cosf(2.0f * M_PI * v);
        }
        return g_sim.temp_c + noise;
    }

    float readFahrenheit() { return readCelsius() * 9.0f / 5.0f + 32.0f; }
};

// ================================================================
//  API SIMULATORE
// ================================================================
void simulator_init();
void simulator_tick(uint32_t dt_ms);
void simulator_set_relay(bool base_on, bool cielo_on);
void simulator_print_status();
void simulator_reset_thermal();
/** Chiamare quando l'utente spegne BASE/CIELO (evita riaccensione da autostart 8s). */
void simulator_user_turned_heat_off(void);

void simulator_test_tick(uint32_t now_ms);

void simulator_notify_shutdown(int reason_int);
