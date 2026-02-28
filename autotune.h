/**
 * autotune.h — Forno Pizza Controller v15
 * ================================================================
 * PID Autotune con libreria PID_AutoTune_v0 (Brett Beauregard)
 *
 * INSTALLAZIONE:
 *   Arduino Library Manager → cerca "PID AutoTune" → installa
 *   "PID AutoTune Library" by Brett Beauregard
 *
 * FUNZIONAMENTO:
 *   1. Utente imposta autotune_split (% potenza Base, default 50)
 *   2. START  → entra in modalità AUTOTUNE
 *      - Sospende PID normale
 *      - Usa un solo sensore (Cielo, come in modalità SINGLE)
 *      - Accende Base e Cielo con la split impostata
 *      - La libreria oscilla attorno al setpoint_cielo corrente
 *   3. Dopo ~5 cicli completi → calcola Kp/Ki/Kd
 *      - Applica gli stessi valori a Base e Cielo
 *      - Salva su NVS
 *      - Torna al PID normale
 *   4. STOP   → interrompe, ripristina parametri precedenti
 *
 * OUTPUT STEP:
 *   La libreria usa un output step (relay bang-bang) per misurare
 *   le oscillazioni. L'output è mappato su potenza relay 0-100%.
 *   AUTOTUNE_OUTPUT_STEP = ampiezza oscillazione relay (default 50%)
 *   AUTOTUNE_NOISE_BAND  = banda morta temperatura (default 2°C)
 *   AUTOTUNE_LOOKBACK    = secondi di storia (default 20s)
 * ================================================================
 */

#pragma once
#include "ui.h"

// ================================================================
//  PARAMETRI AUTOTUNE — modificabili
// ================================================================
#define AUTOTUNE_OUTPUT_STEP   50.0   // % ampiezza step relay (0-100)
#define AUTOTUNE_NOISE_BAND    2.0    // °C banda morta
#define AUTOTUNE_LOOKBACK_S    20     // secondi lookback
#define AUTOTUNE_SETPOINT_OFFSET  10.0  // parte 10°C sotto il setpoint corrente

// ================================================================
//  API
// ================================================================
void autotune_start();   // avvia — chiamato da Task_PID o callback MQTT
void autotune_stop();    // interrompe
void autotune_run(float temp_cielo, uint32_t now_ms);
                         // chiamato ogni ciclo in Task_PID
bool autotune_is_running();
