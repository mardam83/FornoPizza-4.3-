/**
 * nvs_storage.h — Persistenza NVS — Forno Pizza Controller v20
 * ================================================================
 * Salva su flash NVS:
 *   - Setpoint temperatura Base e Cielo
 *   - Parametri PID (Kp, Ki, Kd) per Base e Cielo
 *   - Modalità SINGLE/DUAL sensore
 *   - Percentuale potenza indipendente per Base (0-100%)
 *   - Percentuale potenza indipendente per Cielo (0-100%)
 * ================================================================
 */

#pragma once
#include <Preferences.h>

#define NVS_NAMESPACE   "forno"

#define NVS_SET_BASE    "set_base"
#define NVS_SET_CIELO   "set_cielo"
#define NVS_KP_BASE     "kp_base"
#define NVS_KI_BASE     "ki_base"
#define NVS_KD_BASE     "kd_base"
#define NVS_KP_CIELO    "kp_cielo"
#define NVS_KI_CIELO    "ki_cielo"
#define NVS_KD_CIELO    "kd_cielo"
#define NVS_SINGLE_MODE "single_mode"   // 0=DUAL, 1=SINGLE
#define NVS_PCT_BASE    "pct_base"      // % potenza Base  (0..100)
#define NVS_PCT_CIELO   "pct_cielo"     // % potenza Cielo (0..100)

#define DEFAULT_SET_BASE    250.0f
#define DEFAULT_SET_CIELO   300.0f
#define DEFAULT_KP_BASE     3.0f
#define DEFAULT_KI_BASE     0.02f
#define DEFAULT_KD_BASE     2.0f
#define DEFAULT_KP_CIELO    2.5f
#define DEFAULT_KI_CIELO    0.03f
#define DEFAULT_KD_CIELO    1.5f
#define DEFAULT_SINGLE_MODE 1     // SINGLE di default
#define DEFAULT_PCT_BASE    100   // 100% = nessuna riduzione
#define DEFAULT_PCT_CIELO   100

struct NVSData {
  float set_base, set_cielo;
  float kp_base, ki_base, kd_base;
  float kp_cielo, ki_cielo, kd_cielo;
  int   single_mode;
  int   pct_base;    // % scala output PID resistenza BASE  (0..100)
  int   pct_cielo;   // % scala output PID resistenza CIELO (0..100)
};

class NVSStorage {
public:
  bool load(NVSData& d) {
    if (!_prefs.begin(NVS_NAMESPACE, true)) {
      Serial.println("[NVS] Errore lettura — uso default");
      _setDefaults(d); return false;
    }
    d.set_base    = _prefs.getFloat(NVS_SET_BASE,    DEFAULT_SET_BASE);
    d.set_cielo   = _prefs.getFloat(NVS_SET_CIELO,   DEFAULT_SET_CIELO);
    d.kp_base     = _prefs.getFloat(NVS_KP_BASE,     DEFAULT_KP_BASE);
    d.ki_base     = _prefs.getFloat(NVS_KI_BASE,     DEFAULT_KI_BASE);
    d.kd_base     = _prefs.getFloat(NVS_KD_BASE,     DEFAULT_KD_BASE);
    d.kp_cielo    = _prefs.getFloat(NVS_KP_CIELO,    DEFAULT_KP_CIELO);
    d.ki_cielo    = _prefs.getFloat(NVS_KI_CIELO,    DEFAULT_KI_CIELO);
    d.kd_cielo    = _prefs.getFloat(NVS_KD_CIELO,    DEFAULT_KD_CIELO);
    d.single_mode = _prefs.getInt  (NVS_SINGLE_MODE, DEFAULT_SINGLE_MODE);
    d.pct_base    = _prefs.getInt  (NVS_PCT_BASE,    DEFAULT_PCT_BASE);
    d.pct_cielo   = _prefs.getInt  (NVS_PCT_CIELO,   DEFAULT_PCT_CIELO);
    _prefs.end();
    _validate(d);
    Serial.printf("[NVS] Caricato: mode=%s set=%.0f/%.0f pct=%d%%/%d%%\n",
      d.single_mode ? "SINGLE" : "DUAL",
      d.set_base, d.set_cielo, d.pct_base, d.pct_cielo);
    return true;
  }

  bool save(const NVSData& d) {
    if (!_prefs.begin(NVS_NAMESPACE, false)) {
      Serial.println("[NVS] Errore scrittura"); return false;
    }
    _prefs.putFloat(NVS_SET_BASE,    d.set_base);
    _prefs.putFloat(NVS_SET_CIELO,   d.set_cielo);
    _prefs.putFloat(NVS_KP_BASE,     d.kp_base);
    _prefs.putFloat(NVS_KI_BASE,     d.ki_base);
    _prefs.putFloat(NVS_KD_BASE,     d.kd_base);
    _prefs.putFloat(NVS_KP_CIELO,    d.kp_cielo);
    _prefs.putFloat(NVS_KI_CIELO,    d.ki_cielo);
    _prefs.putFloat(NVS_KD_CIELO,    d.kd_cielo);
    _prefs.putInt  (NVS_SINGLE_MODE, d.single_mode);
    _prefs.putInt  (NVS_PCT_BASE,    d.pct_base);
    _prefs.putInt  (NVS_PCT_CIELO,   d.pct_cielo);
    _prefs.end();
    Serial.printf("[NVS] Salvato: mode=%s set=%.0f/%.0f pct=%d%%/%d%%\n",
      d.single_mode ? "SINGLE" : "DUAL",
      d.set_base, d.set_cielo, d.pct_base, d.pct_cielo);
    return true;
  }

  void reset() {
    if (_prefs.begin(NVS_NAMESPACE, false)) { _prefs.clear(); _prefs.end(); }
  }

private:
  Preferences _prefs;

  void _setDefaults(NVSData& d) {
    d.set_base = DEFAULT_SET_BASE;  d.set_cielo = DEFAULT_SET_CIELO;
    d.kp_base  = DEFAULT_KP_BASE;  d.ki_base   = DEFAULT_KI_BASE;  d.kd_base  = DEFAULT_KD_BASE;
    d.kp_cielo = DEFAULT_KP_CIELO; d.ki_cielo  = DEFAULT_KI_CIELO; d.kd_cielo = DEFAULT_KD_CIELO;
    d.single_mode = DEFAULT_SINGLE_MODE;
    d.pct_base    = DEFAULT_PCT_BASE;
    d.pct_cielo   = DEFAULT_PCT_CIELO;
  }

  void _validate(NVSData& d) {
    if (d.set_base  < 50 || d.set_base  > 500) d.set_base  = DEFAULT_SET_BASE;
    if (d.set_cielo < 50 || d.set_cielo > 500) d.set_cielo = DEFAULT_SET_CIELO;
    if (d.kp_base   < 0  || d.kp_base   > 20)  d.kp_base   = DEFAULT_KP_BASE;
    if (d.kp_cielo  < 0  || d.kp_cielo  > 20)  d.kp_cielo  = DEFAULT_KP_CIELO;
    if (d.ki_base   < 0  || d.ki_base   > 1)   d.ki_base   = DEFAULT_KI_BASE;
    if (d.ki_cielo  < 0  || d.ki_cielo  > 1)   d.ki_cielo  = DEFAULT_KI_CIELO;
    if (d.kd_base   < 0  || d.kd_base   > 20)  d.kd_base   = DEFAULT_KD_BASE;
    if (d.kd_cielo  < 0  || d.kd_cielo  > 20)  d.kd_cielo  = DEFAULT_KD_CIELO;
    if (d.single_mode < 0 || d.single_mode > 1) d.single_mode = DEFAULT_SINGLE_MODE;
    if (d.pct_base  < 0 || d.pct_base  > 100)   d.pct_base  = DEFAULT_PCT_BASE;
    if (d.pct_cielo < 0 || d.pct_cielo > 100)   d.pct_cielo = DEFAULT_PCT_CIELO;
  }
};
