#pragma once
#include <PID_v1.h>
#include <Arduino.h>
#include "hardware.h"

#define PID_WINDOW_MS 30000   // Periodo finestra relay: 30 secondi
#define PID_SAMPLE_MS 500

class PIDController {
public:
  PIDController(double* in, double* out, double* sp, double kp, double ki, double kd)
    : _pid(in, out, sp, kp, ki, kd, DIRECT), _output(out), _win(0) {}

  void begin() {
    _pid.SetOutputLimits(0, 100);
    _pid.SetSampleTime(PID_SAMPLE_MS);
    _pid.SetMode(MANUAL);
    *_output = 0;
    _win = millis();
  }

  void setEnabled(bool en) {
    if (en) { _pid.SetMode(AUTOMATIC); _win = millis(); }
    else    { _pid.SetMode(MANUAL);    *_output = 0; }
  }

  void setTunings(double kp, double ki, double kd) { _pid.SetTunings(kp, ki, kd); }
  void compute() { _pid.Compute(); }

  // ----------------------------------------------------------------
  //  updateRelay — applica duty cycle con:
  //    pct       : 0-100, scala il PID output (100% = nessuna riduzione)
  //    duty_min  : sotto questa soglia il relay non scatta (protegge relay)
  //    duty_max  : sopra questa soglia resta sempre ON
  // ----------------------------------------------------------------
  void updateRelay(bool en, int pin, bool inv, bool& st, int pct = 100) {
    if (!en) {
      if (st) { st = false; _writeRelay(pin, inv, false); }
      return;
    }
    unsigned long now = millis();
    if (now - _win >= PID_WINDOW_MS) _win = now;

    // Scala: output_scaled = pid_out * pct/100
    double scaled = (*_output) * pct / 100.0;

    // Clamp con duty min/max
    double duty_pct;
    if (scaled < RELAY_DUTY_MIN_PCT)       duty_pct = 0.0;   // troppo basso → spento
    else if (scaled > RELAY_DUTY_MAX_PCT)  duty_pct = 100.0; // quasi pieno → sempre ON
    else                                   duty_pct = scaled;

    unsigned long on_ms = (unsigned long)(duty_pct / 100.0 * PID_WINDOW_MS);
    bool s = (now - _win < on_ms);
    if (s != st) { st = s; _writeRelay(pin, inv, s); }
  }

private:
  PID _pid;
  double* _output;
  unsigned long _win;

  static void _writeRelay(int pin, bool inv, bool state) {
    digitalWrite(pin, (state ^ inv) ? HIGH : LOW);
  }
};
