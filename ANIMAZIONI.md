# Animazioni UI — Forno Pizza Controller v22-S3
## File modificati in questa versione

| File | Stato | Descrizione |
|---|---|---|
| `ui_animations.h` | ✏️ AGGIORNATO | +8 animazioni nuove (da 12 a 20 totali) |
| `ui.cpp` | ✏️ AGGIORNATO | Fan breathing, preheat bar, graph intro, autotune bar |
| `ui_events.cpp` | ✏️ AGGIORNATO | Setpoint drag feedback, timer countdown pulse |
| `ui_wifi.cpp` | ✏️ AGGIORNATO/COMPLETO | OTA bar animata (anim 13) |
| `wifi_mqtt.cpp` | invariato rispetto v21 | Blink WiFi già integrato |

---

## Tutte le 20 animazioni

### v21 — Originali (12)

| # | Funzione | Effetto | Trigger |
|---|---|---|---|
| 1 | `anim_fade_screen()` | Opacity 0→255, 280ms | Ogni cambio schermata |
| 2 | `anim_pulse_temp_label()` | Flash 255→150→255, 120ms | Variazione temp >0.9°C |
| 3 | `anim_glow_relay_btn()` | Border 2↔4px loop 550ms | relay ON |
| 4 | `anim_stop_glow()` | Reset border 2px | relay OFF |
| 5 | `cb_anim_bounce` | Zoom 100→85%, 90ms | Bottoni ± PRESSED |
| 6 | `anim_wifi_blink_start()` | Opacity 255→60 loop 400ms | WiFi.begin() |
| 7 | `anim_wifi_blink_stop()` | Ripristina opacity | Connesso/Timeout |
| 8 | `anim_set_bar()` | LV_ANIM_ON barre PID | Ogni refresh 100ms |
| 9 | `anim_relay_led_on()` | 2 flash rapidi 80ms | relay OFF→ON |
| 10 | `anim_arc_set_value()` | Arco 400ms ease-out | Cambio setpoint |
| 11 | `anim_safety_banner()` | Flash rosso 200ms loop | safety_shutdown |
| 12 | `cb_anim_card_press` | Zoom card 100→94%, 80ms | Card ricetta PRESSED |

### v22 — Nuove (8)

| # | Funzione | Effetto | Trigger |
|---|---|---|---|
| 13 | `anim_ota_bar_update()` | Barra OTA LV_ANIM_ON + pulse label | Ogni chunk OTA |
| 14 | `anim_preheat_bar()` | Barra 0→100% con colore termico (blu→arancio→verde) | Preheat attivo |
| 15 | `anim_fan_breathing_start/stop()` | LED ventola respira 150↔255, 1200ms | fan ON/OFF |
| 16 | `anim_toast_show()` | Overlay fade-in 300ms, sosta 2s, fade-out 300ms | Notifica MQTT/save |
| 17 | `anim_timer_countdown_pulse()` | Label rossa pulsa 500ms loop | Timer <60 secondi |
| 18 | `anim_graph_intro()` | Chart slide-in -20px→0 in 350ms, delay 80ms | Apertura schermata grafico |
| 19 | `anim_autotune_bar_start/stop()` | Barra indeterminate 0→100→0 loop 1500ms | AutotuneStatus::RUNNING |
| 20 | `anim_setpoint_drag_feedback()` | Flash bianco→giallo 200ms ease-out | Bottoni +/- setpoint |

---

## Widget nuovi da aggiungere in build_main() e build_pid()

Questi widget sono già creati automaticamente dal codice aggiornato:

| Widget | Schermata | Posizione | Scopo |
|---|---|---|---|
| `ui_BarPreheatBase` | MAIN | x=2, y=211, w=234, h=4 | Barra preheat BASE (anim 14) |
| `ui_BarPreheatCielo` | MAIN | x=244, y=211, w=234, h=4 | Barra preheat CIELO (anim 14) |
| `ui_BarAutotune` | PID_BASE | x=10, y=200, w=460, h=8 | Barra autotune (anim 19) |

---

## Come usare il toast (ANIM 16)

```cpp
// In qualsiasi punto del codice UI (Task_LVGL o callback):
extern void ui_show_toast(const char* msg);

// Esempi di utilizzo:
ui_show_toast(LV_SYMBOL_OK " Ricetta applicata");
ui_show_toast(LV_SYMBOL_WIFI " MQTT: setpoint aggiornato");
ui_show_toast(LV_SYMBOL_SAVE " Parametri PID salvati");
```

---

## Impatto CPU (ESP32-S3 @ 240 MHz)

| Animazione | CPU |
|---|---|
| Fan breathing (loop) | ~0.5% |
| Preheat bar (refresh) | ~0.3% |
| Autotune bar (loop) | ~0.5% |
| Toast (transient) | ~0.3% |
| Timer pulse (loop) | ~0.3% |
| Graph intro (one-shot) | <0.1% |
| OTA bar (per chunk) | ~0.2% |
| Setpoint feedback (one-shot) | <0.1% |
| **Tutte v21+v22 worst case** | **~10%** |

---

## Requisiti lv_conf.h

```c
#define LV_USE_ANIMATION  1   // obbligatorio (default ON)
#define LV_USE_TRANSFORM  1   // obbligatorio per bounce/zoom (default ON)
// Opzionale per future espansioni:
// #define LV_USE_SPINNER  1  // spinner OTA (attualmente non usato)
```
