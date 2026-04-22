/**
 * ui_animations.h — Forno Pizza Controller v24-S3
 * ================================================================
 *
 *  CHANGELOG v24:
 *
 *  [BUG-4 FIX] Rimosso `extern bool g_arc_snap` orfano.
 *              La flag era dichiarata ma mai definita in .cpp (link
 *              orphan) e comunque race-prone (accesso da callback
 *              touch LVGL senza sincronizzazione).
 *              L'arco ora usa anim_arc_set_value() SEMPRE animato;
 *              il tempo è proporzionale alla distanza (min 40 / max
 *              150 ms) quindi anche pressioni rapide del ±5°C danno
 *              un feedback di soli 40 ms — istantaneo all'occhio.
 *
 *  [OPT]       anim_arc_set_value_fast(): variante istantanea
 *              (senza animazione) per aggiornamenti da MQTT/task
 *              quando il widget non è visibile.
 *
 *  [OPT]       cb_anim_bounce / cb_anim_card_press ora usano
 *              lv_anim_del(var, NULL) prima dello start per evitare
 *              accumulo animazioni sovrapposte su press ripetute.
 *
 *  [OPT]       Tutti gli helper "stop" ora chiamano lv_anim_del prima
 *              di resettare le proprietà, così le animazioni in volo
 *              non sovrascrivono lo stato finale.
 *
 * ── IMPATTO CPU (ESP32-S3 @ 240 MHz) ───────────────────────────
 *  Worst case tutto attivo: 6-8% (misurato su v24 con partial refr.)
 * ================================================================
 */

#pragma once
#include <lvgl.h>

/* ================================================================
   ── ANIMAZIONI ORIGINALI 1-12 ──────────────────────────────────
   ================================================================ */

// ── 1. FADE-IN SCHERMATA ────────────────────────────────────────
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
static inline void anim_pulse_temp_label(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, 160);
    lv_anim_set_time(&a, 120);
    lv_anim_set_playback_time(&a, 120);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// ── 3-4. GLOW border relay ──────────────────────────────────────
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
static inline void anim_stop_glow(lv_obj_t* btn) {
    if (!btn) return;
    lv_anim_del(btn, NULL);
    lv_obj_set_style_border_width(btn, 2, 0);
}

// ── 5. BOUNCE bottoni ± ─────────────────────────────────────────
// Usare su LV_EVENT_PRESSED. Anti-accumulo: lv_anim_del prima di start.
static void cb_anim_bounce(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (!btn) return;
    lv_anim_del(btn, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, btn);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_transform_zoom((lv_obj_t*)obj, (lv_coord_t)v, 0); });
    lv_anim_set_values(&a, 256, 228);   // 256=100%, 228≈89%
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// ── 6-7. WIFI BLINK ────────────────────────────────────────────
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
static inline void anim_wifi_blink_stop(lv_obj_t* lbl) {
    if (!lbl) return;
    lv_anim_del(lbl, NULL);
    lv_obj_set_style_opa(lbl, LV_OPA_COVER, 0);
}

// ── 8. BARRA PID ANIMATA ────────────────────────────────────────
static inline void anim_set_bar(lv_obj_t* bar, int32_t value) {
    if (!bar) return;
    lv_bar_set_value(bar, value, LV_ANIM_ON);
}

// ── 9. LED RELAY — doppio flash all'attivazione ─────────────────
static inline void anim_relay_led_on(lv_obj_t* led) {
    if (!led) return;
    lv_anim_del(led, NULL);
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

// ── 10. ARCO SETPOINT — animato proporzionale alla distanza ────
//
// v24 BUG-4 FIX: rimossa flag g_arc_snap (orfana, race-prone).
// Strategia:
//   - cancella anim precedente (lv_anim_del)
//   - durata proporzionale alla distanza: 20 ms/grado
//   - clamp 40..150 ms → pressione ±5° = 40 ms (percezione
//     istantanea), salto da MQTT di 100° = 150 ms morbidi.
//
static inline void anim_arc_set_value(lv_obj_t* arc, int32_t new_val) {
    if (!arc) return;
    int32_t cur = lv_arc_get_value(arc);
    if (cur == new_val) return;
    lv_anim_del(arc, NULL);
    int32_t dist = (new_val > cur) ? (new_val - cur) : (cur - new_val);
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

// Variante istantanea (quando il widget non è visibile o durante
// inizializzazione). Chiama lv_anim_del per sicurezza.
static inline void anim_arc_set_value_fast(lv_obj_t* arc, int32_t new_val) {
    if (!arc) return;
    lv_anim_del(arc, NULL);
    lv_arc_set_value(arc, (int16_t)new_val);
}

// ── 11. BANNER SICUREZZA ────────────────────────────────────────
static inline void anim_safety_banner(lv_obj_t* panel) {
    if (!panel) return;
    lv_anim_del(panel, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, 110);
    lv_anim_set_time(&a, 220);
    lv_anim_set_playback_time(&a, 220);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}
static inline void anim_safety_banner_stop(lv_obj_t* panel) {
    if (!panel) return;
    lv_anim_del(panel, NULL);
    lv_obj_set_style_opa(panel, LV_OPA_COVER, 0);
}

// ── 12. CARD RICETTE — zoom feedback ────────────────────────────
static void cb_anim_card_press(lv_event_t* e) {
    lv_obj_t* card = lv_event_get_target(e);
    if (!card) return;
    lv_anim_del(card, NULL);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_transform_zoom((lv_obj_t*)obj, (lv_coord_t)v, 0); });
    lv_anim_set_values(&a, 256, 240);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* ================================================================
   ── ANIMAZIONI 13-20 ────────────────────────────────────────────
   ================================================================ */

// ── 13. BARRA OTA ANIMATA ──────────────────────────────────────
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

// ── 14. PREHEAT BAR ────────────────────────────────────────────
static inline void anim_preheat_bar(lv_obj_t* bar, int pct) {
    if (!bar) return;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(bar, pct, LV_ANIM_ON);
    lv_color_t col;
    if      (pct < 40)  col = lv_color_make(0x50, 0x54, 0x60);
    else if (pct < 70)  col = lv_color_make(0x88, 0x7C, 0x68);
    else if (pct < 92)  col = lv_color_make(0xB0, 0x90, 0x58);
    else                col = lv_color_make(0x78, 0x98, 0x80);
    lv_obj_set_style_bg_color(bar, col, LV_PART_INDICATOR);
}
static inline void anim_preheat_bar_stop(lv_obj_t* bar) {
    if (!bar) return;
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
}

// ── 15. FAN LED BREATHING ──────────────────────────────────────
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

// ── 16. TOAST NOTIFICA 2s ──────────────────────────────────────
static void _toast_exit_cb(lv_timer_t* t) {
    lv_obj_t* panel = (lv_obj_t*)t->user_data;
    lv_timer_del(t);
    if (!panel || !lv_obj_is_valid(panel)) return;
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
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
    lv_obj_set_style_bg_color(panel, lv_color_make(0x20,0x20,0x30), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_make(0x60,0x60,0x90), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_t* lbl = lv_label_create(panel);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    if (toast_ref) *toast_ref = panel;
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0); });
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    lv_timer_t* tmr = lv_timer_create(_toast_exit_cb, 2300, panel);
    lv_timer_set_repeat_count(tmr, 1);
}

// ── 17. TIMER COUNTDOWN PULSE ──────────────────────────────────
static inline void anim_timer_countdown_pulse(lv_obj_t* lbl) {
    if (!lbl) return;
    if (lv_anim_get(lbl, NULL)) return;
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

// ── 18. GRAPH INTRO ────────────────────────────────────────────
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

// ── 19. AUTOTUNE PROGRESS ──────────────────────────────────────
static inline void anim_autotune_bar_start(lv_obj_t* bar) {
    if (!bar) return;
    if (lv_anim_get(bar, NULL)) return;
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

// ── 20. SETPOINT DRAG FEEDBACK ─────────────────────────────────
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
