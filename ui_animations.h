/**
 * ui_animations.h — Forno Pizza Controller v23-S3
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
 *  10.  anim_arc_set_value()         Arco setpoint — FIX v23: istantaneo+snap
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
 * ── FIX v23 ─────────────────────────────────────────────────────
 *  anim_arc_set_value: cancella anim precedente, tempo proporzionale
 *  alla distanza (max 150ms). Elimina il lag su pressioni rapide.
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

// ── 10. ARCO SETPOINT ANIMATO — FIX v23b ───────────────────────
// FLAG g_arc_snap:
//   Settare a true prima di chiamare ui_refresh_temp() da un callback
//   utente (cb_base_plus/minus, cb_cielo_plus/minus) per ottenere
//   aggiornamento istantaneo dell'arco senza animazione.
//   ui_refresh_temp() resetta il flag automaticamente dopo l'uso.
//
//   In ui_events.cpp:
//     g_arc_snap = true;
//     refresh_active();   // → ui_refresh_temp → anim_arc_set_value
//                         //   vede g_arc_snap=true → snap istantaneo
//                         //   resetta g_arc_snap=false
//
// Quando g_arc_snap=false (default): animazione fluida 40-150ms
// proporzionale alla distanza — usata per aggiornamenti da MQTT/task.
extern bool g_arc_snap;   // definito in ui.cpp (o ui_events.cpp)

static inline void anim_arc_set_value(lv_obj_t* arc, int32_t new_val) {
    if (!arc) return;
    int32_t cur = lv_arc_get_value(arc);
    if (cur == new_val) return;
    // Snap istantaneo se richiesto dall'utente (bottoni ±)
    if (g_arc_snap) {
        lv_anim_del(arc, NULL);
        lv_arc_set_value(arc, (int16_t)new_val);
        g_arc_snap = false;   // reset: un solo snap per chiamata
        return;
    }
    // Cancella animazione in corso — evita accumulo lag
    lv_anim_del(arc, NULL);
    int32_t dist = (new_val > cur) ? (new_val - cur) : (cur - new_val);
    // Tempo proporzionale: 20ms/grado, minimo 40ms, massimo 150ms
    uint32_t t = (uint32_t)(dist * 20);
    if (t < 40)  t = 40;
    if (t > 150) t = 150;
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_arc_set_value((lv_obj_t*)obj, (int16_t)v); });
    lv_anim_set_values(&a, cur, new_val);
    lv_anim_set_time(&a, t);
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
static inline void anim_ota_bar_update(lv_obj_t* bar, lv_obj_t* lbl, int pct) {
    if (!bar) return;
    lv_bar_set_value(bar, pct, LV_ANIM_ON);
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
        lv_anim_del(lbl, NULL);
        lv_obj_set_style_opa(lbl, LV_OPA_COVER, 0);
    }
}

// ── 14. BARRA PREHEAT con colore termico ────────────────────────
// Barra 0→100% con colore che vira blu→arancio→verde.
static inline void anim_preheat_bar(lv_obj_t* bar, int pct) {
    if (!bar) return;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(bar, pct, LV_ANIM_ON);
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
// ciclo 1200ms) invece di restare fisso.
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
// Overlay temporaneo: fade-in 300ms, sosta 2s, fade-out 300ms.
static void _toast_exit_cb(lv_timer_t* t) {
    lv_obj_t* panel = (lv_obj_t*)t->user_data;
    lv_timer_del(t);
    if (!panel || !lv_obj_is_valid(panel)) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
    });
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    // deleted_cb per distruggere il panel al termine del fade-out
    lv_anim_set_deleted_cb(&a, [](lv_anim_t* aa){
        lv_obj_t* p = (lv_obj_t*)aa->var;
        if (p && lv_obj_is_valid(p)) lv_obj_del(p);
    });
    lv_anim_start(&a);
}

static inline void anim_toast_show(lv_obj_t* parent, const char* msg,
                                   lv_obj_t** toast_ref) {
    if (!parent || !msg) return;
    if (toast_ref && *toast_ref && lv_obj_is_valid(*toast_ref)) {
        lv_obj_del(*toast_ref);
        *toast_ref = NULL;
    }
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 320, 40);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(panel, lv_color_make(0x20, 0x20, 0x30), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_make(0x60, 0x60, 0x90), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_t* lbl = lv_label_create(panel);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    if (toast_ref) *toast_ref = panel;
    // Fade-in
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    // Timer per fade-out dopo 2s
    lv_timer_t* tmr = lv_timer_create(_toast_exit_cb, 2300, panel);
    lv_timer_set_repeat_count(tmr, 1);
}

// ── 17. TIMER COUNTDOWN PULSE (<60 secondi) ─────────────────────
// Quando il timer <60sec la label pulsa in rosso a 500ms.
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
// Il chart "scende" di 20px con ease-out in 350ms, delay 80ms.
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
// Barra oscillante 0→100→0 ogni 1500ms durante autotune.
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
// Flash bianco→giallo 200ms ease-out al cambio setpoint.
// Chiamare da cb_base_plus/minus e cb_cielo_plus/minus.
static inline void anim_setpoint_drag_feedback(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        uint8_t g = (uint8_t)(v * 224 / 255);
        lv_obj_set_style_text_color((lv_obj_t*)obj,
            lv_color_make(0xFF, g, 0), 0);
    });
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}
