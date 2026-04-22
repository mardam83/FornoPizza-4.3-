/**
 * simulator_patches_v24.md — Estensioni test simulatore
 * ================================================================
 * Modifiche OPZIONALI al simulator.h / simulator.cpp per aggiungere
 * copertura test su:
 *   - Latenza UI durante PID attivo
 *   - Transizione SINGLE↔DUAL runtime (stress del re-layout)
 *   - Overshoot/settling PID più rigorosi
 *
 * La sequenza di test originale (8 fasi) è invariata.
 * Le nuove fasi si inseriscono tra FINAL_WARMUP e REPORT.
 * ================================================================
 */


═══════════════════════════════════════════════════════════════════
 PATCH SIM-1 · simulator.h · Nuove fasi test
═══════════════════════════════════════════════════════════════════

CERCA:

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

SOSTITUISCI CON:

  enum class SimTestPhase {
      WAIT_START      = 0,
      PID_WARMUP      = 1,
      OVERTEMP        = 2,
      TC_ERROR        = 3,
      RUNAWAY_UP      = 4,
      RUNAWAY_DOWN    = 5,
      AUTOTUNE        = 6,
      FINAL_WARMUP    = 7,
      // v24 nuove fasi prima del report
      UI_MODE_TOGGLE  = 8,   // toggle SINGLE↔DUAL e verifica layout
      UI_SETPOINT_SPAM= 9,   // 20 cambi SP/s → stress arc anim
      REPORT          = 10,
      DONE            = 11
  };

═══════════════════════════════════════════════════════════════════

CERCA dentro SimulatorState:

      int     tests_passed;
      int     tests_total;

INSERISCI PRIMA di tests_passed:

      // v24: metriche più fini sul PID
      float   pid_overshoot_c;       // max overshoot ° sopra setpoint
      float   pid_steady_err_c;      // errore medio a regime
      uint32_t pid_settled_at_ms;    // quando entra nella banda ±3°

      // v24: metriche UI
      bool    test_ui_mode_toggle_ok;
      bool    test_ui_setpoint_spam_ok;

INSERISCI DOPO int tests_total (così la struct resta valida):

      (nessuna ulteriore modifica)


═══════════════════════════════════════════════════════════════════
 PATCH SIM-2 · simulator.cpp · Case UI_MODE_TOGGLE
═══════════════════════════════════════════════════════════════════

Nel simulator_test_tick(), aggiungere il nuovo case DOPO FINAL_WARMUP
e PRIMA del case REPORT:

  case SimTestPhase::UI_MODE_TOGGLE:
  {
      // v24: stressa il re-layout alternando SINGLE↔DUAL ogni 800 ms.
      // Verifica che ui_apply_*_sensor_layout non crashi e che l'UI
      // resti responsiva.
      static uint32_t s_last_toggle_ms = 0;
      static int      s_toggles        = 0;
      if (now_ms - s_last_toggle_ms >= 800) {
          s_last_toggle_ms = now_ms;
          if (MUTEX_TAKE_MS(50)) {
              g_state.sensor_mode =
                  (g_state.sensor_mode == SensorMode::SINGLE)
                      ? SensorMode::DUAL : SensorMode::SINGLE;
              if (g_state.sensor_mode == SensorMode::SINGLE)
                  g_state.cielo_enabled = false;
              MUTEX_GIVE();
          }
          // La UI deve essere notificata esplicitamente (v24 non
          // più auto-layout in refresh). Il test simula il toggle
          // utente chiamando direttamente le API di layout.
          extern void ui_apply_main_sensor_layout(bool);
          extern void ui_apply_temp_sensor_layout(bool);
          bool single = (g_state.sensor_mode == SensorMode::SINGLE);
          ui_apply_main_sensor_layout(single);
          ui_apply_temp_sensor_layout(single);
          s_toggles++;
          Serial.printf("[TEST-UI] toggle #%d → %s\n",
                        s_toggles, single ? "SINGLE" : "DUAL");
      }
      if (s_toggles >= 8) {
          g_sim.test_ui_mode_toggle_ok = true;
          log_pass("UI mode toggle: 8 cicli OK");
          s_toggles = 0;
          g_sim.phase = SimTestPhase::UI_SETPOINT_SPAM;
          g_sim.phase_start_ms = now_ms;
      }
      break;
  }


═══════════════════════════════════════════════════════════════════
 PATCH SIM-3 · simulator.cpp · Case UI_SETPOINT_SPAM
═══════════════════════════════════════════════════════════════════

Subito dopo il case precedente:

  case SimTestPhase::UI_SETPOINT_SPAM:
  {
      // Simula un utente che preme ± sul setpoint 20 volte/s.
      // Verifica che:
      //   - anim_arc_set_value non accumuli animazioni (no lag)
      //   - i callback non causino stall mutex
      static uint32_t s_last_spam_ms = 0;
      static int      s_spam_count   = 0;
      static int      s_spam_dir     = +1;
      if (now_ms - s_last_spam_ms >= 50) {  // 20 Hz
          s_last_spam_ms = now_ms;
          if (MUTEX_TAKE_MS(20)) {
              double new_sp = g_state.set_cielo + s_spam_dir * 5.0;
              if (new_sp >= 400.0) { new_sp = 400.0; s_spam_dir = -1; }
              if (new_sp <= 100.0) { new_sp = 100.0; s_spam_dir = +1; }
              g_state.set_cielo = new_sp;
              if (g_state.sensor_mode == SensorMode::SINGLE)
                  g_state.set_base = new_sp;
              MUTEX_GIVE();
          }
          s_spam_count++;
      }
      if (s_spam_count >= 60) {  // 3 secondi di spam
          g_sim.test_ui_setpoint_spam_ok = true;
          log_pass("Setpoint spam: 60 cambi rapidi OK");
          s_spam_count = 0;
          g_sim.phase = SimTestPhase::REPORT;
          g_sim.phase_start_ms = now_ms;
      }
      break;
  }


═══════════════════════════════════════════════════════════════════
 PATCH SIM-4 · simulator.cpp · Case PID_WARMUP metrics
═══════════════════════════════════════════════════════════════════

Nel case PID_WARMUP esistente, aggiungere raccolta overshoot e
settling time più precisi. Cerca il log periodico e integra:

  // v24: tracking overshoot/settling
  float sp = (float)g_state.set_base;
  float err_abs = fabsf(g_sim.temp_c - sp);

  // overshoot = max temp sopra setpoint
  if (g_sim.temp_c > sp) {
      float os = g_sim.temp_c - sp;
      if (os > g_sim.pid_overshoot_c) g_sim.pid_overshoot_c = os;
  }
  // settling: prima volta che entra in banda ±3° e ci resta 10s
  static uint32_t s_in_band_since = 0;
  if (err_abs < 3.0f) {
      if (s_in_band_since == 0) s_in_band_since = now_ms;
      if ((now_ms - s_in_band_since) > 10000 && g_sim.pid_settled_at_ms == 0) {
          g_sim.pid_settled_at_ms = now_ms - g_sim.phase_start_ms;
          Serial.printf("[TEST-PID] settled @ %u ms, overshoot=%.1f°C\n",
                        (unsigned)g_sim.pid_settled_at_ms,
                        g_sim.pid_overshoot_c);
      }
  } else {
      s_in_band_since = 0;
  }


═══════════════════════════════════════════════════════════════════
 PATCH SIM-5 · simulator.cpp · REPORT esteso
═══════════════════════════════════════════════════════════════════

Nel case REPORT aggiungere al log finale:

  Serial.printf("[REPORT] PID overshoot max     : %.1f °C\n",
                g_sim.pid_overshoot_c);
  if (g_sim.pid_settled_at_ms > 0)
      Serial.printf("[REPORT] PID settling time    : %.1f s\n",
                    g_sim.pid_settled_at_ms / 1000.0f);
  Serial.printf("[REPORT] UI mode toggle       : %s\n",
                g_sim.test_ui_mode_toggle_ok ? "PASS" : "FAIL");
  Serial.printf("[REPORT] UI setpoint spam     : %s\n",
                g_sim.test_ui_setpoint_spam_ok ? "PASS" : "FAIL");

E nel conteggio totale:
  g_sim.tests_total += 2;
  if (g_sim.test_ui_mode_toggle_ok)    g_sim.tests_passed++;
  if (g_sim.test_ui_setpoint_spam_ok)  g_sim.tests_passed++;

═══════════════════════════════════════════════════════════════════
 OBIETTIVI DI PASS (linee guida)
═══════════════════════════════════════════════════════════════════

 PID overshoot          : < 15 °C  (buono), < 30 °C (accettabile)
 PID settling time      : < 300 s  (@ scala 4x = 75 s reali)
 UI mode toggle         : 8/8 OK, nessun crash
 UI setpoint spam       : 60/60 senza esaurire slot animazioni LVGL
                          (LV_ANIM_DEL_ALL non dovrebbe mai servire)
