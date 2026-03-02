/**
 * ui_animations.h — Forno Pizza Controller v22-S3
 * ================================================================
 * Libreria centrale di tutte le animazioni LVGL dell'interfaccia.
 *
 * ── ANIMAZIONI v21 (originali) ──────────────────────────────────
 *   1.  anim_fade_screen()           Fade-in schermata 0→255 / 280ms
 *   2.  anim_pulse_temp_label()      Flash label temperatura su cambio
 *   3.  anim_glow_relay_btn()        Pulsazione border 2↔4px relay ON
 *   4.  anim_stop_glow()             Ferma glow, reset border 2px
 *   5.  cb_anim_bounce               Bounce zoom 100→85% bottoni ±
 *   6.  anim_wifi_blink_start()      Lampeggio WiFi durante connessione
 *   7.  anim_wifi_blink_stop()       Ferma lampeggio WiFi
 *   8.  anim_set_bar()               Barre PID fluide LV_ANIM_ON
 *   9.  anim_relay_led_on()          LED relay: 2 flash rapidi ON
 *  10.  anim_arc_set_value()         Arco setpoint transizione 400ms
 *  11.  anim_safety_banner()         Banner sicurezza flash rosso 200ms
 *  11b. anim_safety_banner_stop()    Ferma banner safety
 *  12.  cb_anim_card_press           Zoom card ricette 100→94% al tocco
 *
 * ── ANIMAZIONI v22 (nuove) ──────────────────────────────────────
 *  13.  anim_ota_bar_update()        Barra OTA fluida + pulse label %
 *  14.  anim_preheat_bar()           Barra preheat con colore termico
 *  14b. anim_preheat_bar_stop()      Ferma preheat bar
 *  15.  anim_fan_breathing_start()   LED ventola "respira" 150↔255
 *  15b. anim_fan_breathing_stop()    Ferma breathing ventola
 *  16.  anim_toast_show()            Toast notifica overlay 2s
 *  17.  anim_timer_countdown_pulse() Pulse rosso timer <60 sec
 *  17b. anim_timer_countdown_stop()  Ferma pulse timer
 *  18.  anim_graph_intro()           Chart slide-in apertura grafico
 *  19.  anim_autotune_bar_start()    Barra indeterminate autotune
 *  19b. anim_autotune_bar_stop()     Ferma progress autotune
 *  20.  anim_setpoint_drag_feedback() Flash bianco→giallo cambio SP
 *
 * ── REQUISITI lv_conf.h ─────────────────────────────────────────
 *  LV_USE_ANIMATION  1   (default ON in LVGL 8.3.x)
 *  LV_USE_TRANSFORM  1   (default ON — richiesto per bounce/zoom)
 *
 * ── IMPATTO CPU (ESP32-S3 @ 240 MHz) ───────────────────────────
 *  Worst case tutto attivo: ~8-10%
 * ================================================================
 */

#pragma once
#include <lvgl.h>

/* ================================================================
   ── SEZIONE 1-12: ANIMAZIONI ORIGINALI v21 ─────────────────────
   ================================================================ */

// ── 1. FADE-IN SCHERMATA ────────────────────────────────────────
// Opacity 0→255 in 280ms ease-out. Chiama subito dopo lv_scr_load_anim().
static inline void anim_fade_screen(lv_obj_t* scr) {
    if (!scr) return;
    lv_obj_set_style_opa(scr, LV_OPA_TRANSP, 0);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, scr);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 280);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// ── 2. PULSE LABEL TEMPERATURA ──────────────────────────────────
// Flash opacity 255→150→255 in 120ms+120ms al cambio di valore.
// Usare con soglia ±0.9°C per evitare jitter dal sensore.
static inline void anim_pulse_temp_label(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, 150);
    lv_anim_set_time(&a, 120);
    lv_anim_set_playback_time(&a, 120);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// ── 3. GLOW BORDER — relay attivo ───────────────────────────────
// Border_width 2→4px in loop infinito 550ms quando relay è ON.
static inline void anim_glow_relay_btn(lv_obj_t* btn, lv_color_t col) {
    if (!btn) return;
    lv_anim_del(btn, NULL);
    lv_obj_set_style_border_color(btn, col, 0);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, btn);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_border_width((lv_obj_t*)obj, (lv_coord_t)v, 0); });
    lv_anim_set_values(&a, 2, 4);
    lv_anim_set_time(&a, 550);
    lv_anim_set_playback_time(&a, 550);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// ── 4. STOP GLOW ────────────────────────────────────────────────
static inline void anim_stop_glow(lv_obj_t* btn) {
    if (!btn) return;
    lv_anim_del(btn, NULL);
    lv_obj_set_style_border_width(btn, 2, 0);
}

// ── 5. BOUNCE bottoni ± ─────────────────────────────────────────
// Zoom 100%→85%→100% in 90ms+90ms. Registrare su LV_EVENT_PRESSED.
static void cb_anim_bounce(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (!btn) return;
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, btn);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_transform_zoom((lv_obj_t*)obj, (lv_coord_t)v, 0); });
    lv_anim_set_values(&a, 256, 218);   // 256=100%, 218≈85%
    lv_anim_set_time(&a, 90);
    lv_anim_set_playback_time(&a, 90);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// ── 6. WIFI BLINK start ─────────────────────────────────────────
// Opacity 255→60→255 loop 400ms durante connessione in corso.
static inline void anim_wifi_blink_start(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, 60);
    lv_anim_set_time(&a, 400);
    lv_anim_set_playback_time(&a, 400);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// ── 7. WIFI BLINK stop ──────────────────────────────────────────
static inline void anim_wifi_blink_stop(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    lv_obj_set_style_opa(lbl, LV_OPA_COVER, 0);
}

// ── 8. BARRA PID ANIMATA ────────────────────────────────────────
// Usa LV_ANIM_ON per transizione fluida invece di LV_ANIM_OFF.
static inline void anim_set_bar(lv_obj_t* bar, int32_t value) {
    if (!bar) return;
    lv_bar_set_value(bar, value, LV_ANIM_ON);
}

// ── 9. LED RELAY — doppio flash all'attivazione ─────────────────
// Opacity 255→50→255, 2 ripetizioni, 80ms ciascuna.
static inline void anim_relay_led_on(lv_obj_t* led) {
    if (!led) return;
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, led);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, 50);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// ── 10. ARCO SETPOINT ANIMATO ───────────────────────────────────
// Transizione fluida 400ms ease-out. Sostituisce lv_arc_set_value() diretto.
static inline void anim_arc_set_value(lv_obj_t* arc, int32_t new_val) {
    if (!arc) return;
    int32_t cur = lv_arc_get_value(arc);
    if (cur == new_val) return;
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_arc_set_value((lv_obj_t*)obj, (int16_t)v); });
    lv_anim_set_values(&a, cur, new_val);
    lv_anim_set_time(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// ── 11. BANNER SICUREZZA — flash rosso urgente ──────────────────
// Opacity 255→100→255 loop 200ms. Chiama una volta quando safety=true.
static inline void anim_safety_banner(lv_obj_t* panel) {
    if (!panel) return;
    lv_anim_del(panel, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, 100);
    lv_anim_set_time(&a, 200);
    lv_anim_set_playback_time(&a, 200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static inline void anim_safety_banner_stop(lv_obj_t* panel) {
    if (!panel) return;
    lv_anim_del(panel, NULL);
    lv_obj_set_style_opa(panel, LV_OPA_COVER, 0);
}

// ── 12. CARD RICETTE — zoom feedback al tocco ───────────────────
// Zoom 100→94%→100% in 80ms. Registrare su LV_EVENT_PRESSED.
static void cb_anim_card_press(lv_event_t* e) {
    lv_obj_t* card = lv_event_get_target(e);
    if (!card) return;
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_transform_zoom((lv_obj_t*)obj, (lv_coord_t)v, 0); });
    lv_anim_set_values(&a, 256, 240);   // 256=100%, 240=94%
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}


/* ================================================================
   ── SEZIONE 13-20: ANIMAZIONI NUOVE v22 ────────────────────────
   ================================================================ */

// ── 13. BARRA OTA ANIMATA durante download ──────────────────────
// La barra avanza fluidamente (LV_ANIM_ON) e la label % fa un
// micro-pulse a ogni chunk ricevuto per dare feedback visivo immediato.
//
// INTEGRAZIONE in ui_wifi.cpp → ui_ota_update_progress():
//   Sostituire lv_bar_set_value(ui_OtaBar, p, LV_ANIM_OFF) con:
//   anim_ota_bar_update(ui_OtaBar, ui_OtaBarLbl, p);
// ================================================================
static inline void anim_ota_bar_update(lv_obj_t* bar, lv_obj_t* lbl, int pct) {
    if (!bar) return;
    // Scorrimento fluido della barra
    lv_bar_set_value(bar, pct, LV_ANIM_ON);
    // Micro-pulse sulla label solo durante il download (0 < pct < 100)
    if (lbl && pct > 0 && pct < 100) {
        lv_anim_del(lbl, NULL);
        lv_anim_t a; lv_anim_init(&a);
        lv_anim_set_var(&a, lbl);
        lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
            lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
        lv_anim_set_values(&a, LV_OPA_COVER, 180);
        lv_anim_set_time(&a, 300);
        lv_anim_set_playback_time(&a, 300);
        lv_anim_set_repeat_count(&a, 1);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    } else if (lbl) {
        // A fine download ferma il pulse e torna piena opacità
        lv_anim_del(lbl, NULL);
        lv_obj_set_style_opa(lbl, LV_OPA_COVER, 0);
    }
}

// ── 14. BARRA PREHEAT con colore termico ────────────────────────
// Durante il preriscaldo mostra una barra 0→100% il cui colore
// vira da blu freddo → arancio → verde man mano che la temperatura
// si avvicina al setpoint. Molto più informativo di una label statica.
//
// INTEGRAZIONE in ui.cpp → ui_refresh(), nella sezione preheat:
//   if (s->preheat_base && !s->tc_base_err)
//       anim_preheat_bar(ui_BarPreheatBase,
//                        (int)(s->temp_base / s->set_base * 100.0));
//   else
//       anim_preheat_bar_stop(ui_BarPreheatBase);
//
// NOTA: ui_BarPreheatBase deve essere una lv_bar aggiunta in build_main()
//       sovrapposta al pannello BASE (es. y=215, h=4, w=234, x=2).
// ================================================================
static inline void anim_preheat_bar(lv_obj_t* bar, int pct) {
    if (!bar) return;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(bar, pct, LV_ANIM_ON);
    // Colore termico: blu → arancio chiaro → arancio scuro → verde
    lv_color_t col;
    if      (pct < 40)  col = lv_color_make(0x00, 0x80, 0xFF); // blu freddo
    else if (pct < 70)  col = lv_color_make(0xFF, 0x9E, 0x40); // arancio caldo
    else if (pct < 92)  col = lv_color_make(0xFF, 0x6B, 0x00); // arancio intenso
    else                col = lv_color_make(0x00, 0xE6, 0x76); // verde = pronto!
    lv_obj_set_style_bg_color(bar, col, LV_PART_INDICATOR);
}

static inline void anim_preheat_bar_stop(lv_obj_t* bar) {
    if (!bar) return;
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
}

// ── 15. FAN LED BREATHING ───────────────────────────────────────
// Quando la ventola è ON il LED "respira" lentamente (opacity 150↔255,
// ciclo 1200ms) invece di restare fisso. Indica al volo la ventola attiva.
//
// INTEGRAZIONE in ui.cpp → ui_refresh(), sostituire il blocco fan:
//   static bool s_fan_prev = false;
//   if (s->fan_on && !s_fan_prev) {
//       lv_led_on(ui_LedFan);
//       anim_fan_breathing_start(ui_LedFan);
//   } else if (!s->fan_on && s_fan_prev) {
//       anim_fan_breathing_stop(ui_LedFan);
//       lv_led_off(ui_LedFan);
//   }
//   s_fan_prev = s->fan_on;
// ================================================================
static inline void anim_fan_breathing_start(lv_obj_t* led) {
    if (!led) return;
    lv_anim_del(led, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, led);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, 255, 150);
    lv_anim_set_time(&a, 1200);
    lv_anim_set_playback_time(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static inline void anim_fan_breathing_stop(lv_obj_t* led) {
    if (!led) return;
    lv_anim_del(led, NULL);
    lv_obj_set_style_opa(led, LV_OPA_COVER, 0);
}

// ── 16. TOAST NOTIFICA overlay 2s ───────────────────────────────
// Overlay temporaneo che appare dal basso, resta 2 secondi,
// poi esce con fade-out. Usare per: "MQTT: cmd ricevuto",
// "Ricetta applicata", "Salvato!", ecc.
//
// UTILIZZO:
//   static lv_obj_t* s_toast = NULL;
//   anim_toast_show(lv_scr_act(), "MQTT: comando ricevuto", &s_toast);
//
// Il parametro toast_ref permette di cancellare un toast ancora
// visibile prima di mostrarne uno nuovo. Può essere NULL.
// ================================================================
static void _toast_exit_cb(lv_timer_t* t) {
    // 1. Accedi direttamente a t->user_data invece di usare la funzione
    lv_obj_t* panel = (lv_obj_t*)t->user_data;
    
    // Eliminiamo subito il timer che ha scatenato il callback
    lv_timer_del(t);
    
    if (!panel || !lv_obj_is_valid(panel)) return;

    // Fade-out
    lv_anim_t a; 
    lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); 
    });
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);

    // 2. Correzione Lambda: usiamo dt->user_data per evitare errori di "capture"
    lv_timer_t* del_t = lv_timer_create([](lv_timer_t* dt) {
        lv_obj_t* p = (lv_obj_t*)dt->user_data;
        if (p && lv_obj_is_valid(p)) {
            lv_obj_del(p);
        }
        // In LVGL 8.3, i timer creati con repeat_count 1 si auto-eliminano,
        // ma chiamare lv_timer_del(dt) qui è una sicurezza extra.
        lv_timer_del(dt);
    }, 310, panel);
    
    lv_timer_set_repeat_count(del_t, 1);
}

static inline void anim_toast_show(lv_obj_t* parent_scr,
                                    const char* msg,
                                    lv_obj_t** toast_ref) {
    if (!parent_scr || !msg) return;
    // Cancella toast precedente se ancora vivo
    if (toast_ref && *toast_ref && lv_obj_is_valid(*toast_ref)) {
        lv_obj_del(*toast_ref);
        *toast_ref = NULL;
    }
    // Pannello toast — centrato in basso, z=top
    lv_obj_t* panel = lv_obj_create(parent_scr);
    lv_obj_set_pos(panel, 80, 236);
    lv_obj_set_size(panel, 320, 30);
    lv_obj_set_style_bg_color(panel, lv_color_make(0x10, 0x24, 0x10), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_make(0x00, 0xE6, 0x76), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_shadow_width(panel, 14, 0);
    lv_obj_set_style_shadow_color(panel, lv_color_make(0x00, 0xE6, 0x76), 0);
    lv_obj_set_style_shadow_opa(panel, 120, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_opa(panel, LV_OPA_TRANSP, 0);
    if (toast_ref) *toast_ref = panel;
    // Label messaggio
    lv_obj_t* lbl = lv_label_create(panel);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_make(0x00, 0xE6, 0x76), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
    // Fade-in 300ms
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    // Timer per avviare il fade-out dopo 2000ms
    lv_timer_t* t = lv_timer_create(_toast_exit_cb, 2000, panel);
    lv_timer_set_repeat_count(t, 1);
}

// ── 17. TIMER COUNTDOWN PULSE (<60 secondi) ─────────────────────
// Quando il timer cottura si avvicina allo zero (<60 sec) la label
// pulsa in rosso a 500ms per avvisare l'utente senza suoni.
//
// INTEGRAZIONE in ui_events.cpp → ui_timer_tick_1s():
//   if (s_timer_minutes == 0 && s_timer_seconds <= 60)
//       anim_timer_countdown_pulse(ui_LabelTimerValue);
//   else
//       anim_timer_countdown_stop(ui_LabelTimerValue);
// ================================================================
static inline void anim_timer_countdown_pulse(lv_obj_t* lbl) {
    if (!lbl) return;
    if (lv_anim_get(lbl, NULL)) return;   // già in corso, non riavviare
    lv_obj_set_style_text_color(lbl, lv_color_make(0xFF, 0x30, 0x30), 0);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, 60);
    lv_anim_set_time(&a, 500);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static inline void anim_timer_countdown_stop(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    lv_obj_set_style_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(lbl, lv_color_make(0x80, 0x80, 0xFF), 0);
}

// ── 18. GRAPH INTRO — chart slide-in all'apertura ───────────────
// Il chart "scende" di 20px con ease-out in 350ms, con 80ms di
// delay dopo la transizione schermata, per dare profondità all'UI.
//
// INTEGRAZIONE in ui.cpp → ui_show_screen(), case Screen::GRAPH:
//   lv_scr_load_anim(ui_ScreenGraph, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false);
//   anim_fade_screen(ui_ScreenGraph);
//   anim_graph_intro(ui_Chart);   // ← aggiungere questa riga
// ================================================================
static inline void anim_graph_intro(lv_obj_t* chart) {
    if (!chart) return;
    lv_coord_t orig_y = lv_obj_get_y(chart);
    lv_obj_set_y(chart, orig_y - 20);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, chart);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)v); });
    lv_anim_set_values(&a, orig_y - 20, orig_y);
    lv_anim_set_time(&a, 350);
    lv_anim_set_delay(&a, 80);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// ── 19. AUTOTUNE PROGRESS — barra indeterminate ─────────────────
// Durante l'autotune PID la durata è ignota, quindi si usa una barra
// "indeterminate" che oscilla 0→100→0 ogni 1500ms continuamente.
// Mostra chiaramente che il processo è in corso senza un ETA.
//
// INTEGRAZIONE in ui.cpp → ui_refresh_pid():
//   if (s->autotune_status == AutotuneStatus::RUNNING) {
//       lv_obj_clear_flag(ui_BarAutotune, LV_OBJ_FLAG_HIDDEN);
//       anim_autotune_bar_start(ui_BarAutotune);
//   } else {
//       anim_autotune_bar_stop(ui_BarAutotune, s->autotune_cycles);
//   }
//
// NOTA: ui_BarAutotune va aggiunta in build_pid_base() e build_pid_cielo()
//       (es. y=200, h=8, w=460, x=10, visibile solo durante autotune).
// ================================================================
static inline void anim_autotune_bar_start(lv_obj_t* bar) {
    if (!bar) return;
    if (lv_anim_get(bar, NULL)) return;  // già in corso
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_bar_set_value((lv_obj_t*)obj, (int16_t)v, LV_ANIM_OFF); });
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_playback_time(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static inline void anim_autotune_bar_stop(lv_obj_t* bar, int cycles_done) {
    if (!bar) return;
    lv_anim_del(bar, NULL);
    lv_bar_set_value(bar, cycles_done > 0 ? 100 : 0, LV_ANIM_ON);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
}

// ── 20. SETPOINT DRAG FEEDBACK ──────────────────────────────────
// Ogni volta che il setpoint cambia (bottoni + / −) la label del
// valore impostato fa un rapido flash bianco→giallo (200ms ease-out)
// per confermare visivamente il nuovo valore.
//
// INTEGRAZIONE in ui_events.cpp → cb_base_plus, cb_base_minus,
//                                  cb_cielo_plus, cb_cielo_minus:
//   Aggiungere al termine di ogni callback:
//   anim_setpoint_drag_feedback(ui_TempSetBase);   // o ui_TempSetCielo
// ================================================================
static inline void anim_setpoint_drag_feedback(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    // Parte da bianco puro, poi torna al giallo setpoint in 200ms
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        // v: 255→0 — interpola verde G da 255 a 224, B resta 0
        uint8_t g = (uint8_t)(v * 224 / 255);
        lv_obj_set_style_text_color((lv_obj_t*)obj,
            lv_color_make(0xFF, g, 0), 0);
    });
    lv_anim_set_values(&a, 255, 0);   // 255=bianco, 0=giallo pieno (R=FF G=0→224 B=0)
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}
