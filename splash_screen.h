/**
 * splash_screen.h — Forno Pizza v2 — FIX ANIMAZIONI
 *
 * FIX v2:
 *   1. lv_scr_load_anim → lv_scr_load (istantaneo)
 *      CAUSA BUG: Task_LVGL chiama ui_refresh() mentre la transizione
 *      è in corso → crash LoadProhibited (accesso a widget della schermata
 *      precedente che LVGL ha già de-allocato internamente).
 *      Con full_refresh=1 e double FB il frame viene comunque ridisegnato
 *      completamente ad ogni ciclo — l'animazione di transizione non aggiunge
 *      valore visivo e introduce il race condition.
 *
 *   2. lv_bar_set_value(..., LV_ANIM_ON) → LV_ANIM_OFF
 *      Con LV_ANIM_ON LVGL registra un timer interno per interpolare il valore.
 *      Se splash_set_progress() viene chiamata frequentemente (ogni step del
 *      setup) i timer si accumulano e possono causare comportamento imprevedibile
 *      durante il passaggio alla schermata MAIN.
 *
 *   3. Fade-out splash rimosso (usava lv_anim su opa → race condition con
 *      la schermata MAIN che viene caricata in parallelo).
 *      Sostituito con transizione immediata — il setup è già abbastanza rapido.
 */

#pragma once
#include <lvgl.h>
#include "hardware.h"

// ================================================================
//  COLORI SPLASH
// ================================================================
#define SPLASH_BG        lv_color_make(0x06, 0x06, 0x0E)
#define SPLASH_ORANGE    lv_color_make(0xFF, 0x6B, 0x00)
#define SPLASH_YELLOW    lv_color_make(0xFF, 0xCC, 0x00)
#define SPLASH_RED       lv_color_make(0xFF, 0x22, 0x00)
#define SPLASH_GRAY      lv_color_make(0x60, 0x60, 0x80)
#define SPLASH_DARKGRAY  lv_color_make(0x18, 0x18, 0x28)
#define SPLASH_BAR_BG    lv_color_make(0x12, 0x12, 0x20)

// ================================================================
//  STATO INTERNO
// ================================================================
static lv_obj_t* s_scr     = nullptr;
static lv_obj_t* s_bar     = nullptr;
static lv_obj_t* s_bar_lbl = nullptr;
static lv_obj_t* s_pct_lbl = nullptr;
static lv_obj_t* s_flame1  = nullptr;
static lv_obj_t* s_flame2  = nullptr;
static lv_obj_t* s_flame3  = nullptr;
static bool      s_splash_done = false;

// ================================================================
//  ANIMAZIONE FIAMME — colore ciclico (solo durante splash)
// ================================================================
static void flame_color_anim_cb(void* obj, int32_t val) {
  lv_obj_t* o = (lv_obj_t*)obj;
  lv_color_t c;
  if (val < 50) {
    c = lv_color_mix(SPLASH_ORANGE, SPLASH_RED,    (uint8_t)(val * 5));
  } else {
    c = lv_color_mix(SPLASH_YELLOW, SPLASH_ORANGE, (uint8_t)((val - 50) * 5));
  }
  lv_obj_set_style_bg_color(o, c, 0);
  // Aggiorna anche l'altezza in base allo user_data (altezza base)
  int32_t base_h = (int32_t)(intptr_t)lv_obj_get_user_data(o);
  int32_t h = base_h - (val / 10);
  if (h < base_h - 8) h = base_h - 8;
  lv_obj_set_height(o, h);
}

// ================================================================
//  splash_build_graphics() — costruisce la grafica interna
// ================================================================
static void splash_build_graphics(lv_obj_t* parent) {
  // ── Forno stilizzato ──
  lv_obj_t* body = lv_obj_create(parent);
  lv_obj_set_pos(body, 150, 60);
  lv_obj_set_size(body, 180, 100);
  lv_obj_set_style_bg_color(body, SPLASH_DARKGRAY, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(body, SPLASH_ORANGE, 0);
  lv_obj_set_style_border_width(body, 2, 0);
  lv_obj_set_style_radius(body, 8, 0);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  // Porta forno
  lv_obj_t* door = lv_obj_create(parent);
  lv_obj_set_pos(door, 190, 75);
  lv_obj_set_size(door, 100, 70);
  lv_obj_set_style_bg_color(door, lv_color_make(0x10, 0x10, 0x18), 0);
  lv_obj_set_style_bg_opa(door, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(door, lv_color_make(0x50, 0x50, 0x70), 0);
  lv_obj_set_style_border_width(door, 1, 0);
  lv_obj_set_style_radius(door, 4, 0);
  lv_obj_clear_flag(door, LV_OBJ_FLAG_SCROLLABLE);

  // Base forno
  lv_obj_t* foot = lv_obj_create(parent);
  lv_obj_set_pos(foot, 140, 160);
  lv_obj_set_size(foot, 200, 12);
  lv_obj_set_style_bg_color(foot, SPLASH_DARKGRAY, 0);
  lv_obj_set_style_bg_opa(foot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(foot, 0, 0);
  lv_obj_set_style_radius(foot, 3, 0);
  lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

  // ── Fiamme ──
  s_flame1 = lv_obj_create(parent);
  lv_obj_set_pos(s_flame1, 170, 38);
  lv_obj_set_size(s_flame1, 22, 35);
  lv_obj_set_style_bg_color(s_flame1, SPLASH_RED, 0);
  lv_obj_set_style_bg_opa(s_flame1, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_flame1, 0, 0);
  lv_obj_set_style_radius(s_flame1, 11, 0);
  lv_obj_set_user_data(s_flame1, (void*)35);
  lv_obj_clear_flag(s_flame1, LV_OBJ_FLAG_SCROLLABLE);

  s_flame2 = lv_obj_create(parent);
  lv_obj_set_pos(s_flame2, 228, 28);
  lv_obj_set_size(s_flame2, 28, 45);
  lv_obj_set_style_bg_color(s_flame2, SPLASH_ORANGE, 0);
  lv_obj_set_style_bg_opa(s_flame2, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_flame2, 0, 0);
  lv_obj_set_style_radius(s_flame2, 14, 0);
  lv_obj_set_user_data(s_flame2, (void*)45);
  lv_obj_clear_flag(s_flame2, LV_OBJ_FLAG_SCROLLABLE);

  s_flame3 = lv_obj_create(parent);
  lv_obj_set_pos(s_flame3, 288, 38);
  lv_obj_set_size(s_flame3, 22, 35);
  lv_obj_set_style_bg_color(s_flame3, SPLASH_YELLOW, 0);
  lv_obj_set_style_bg_opa(s_flame3, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_flame3, 0, 0);
  lv_obj_set_style_radius(s_flame3, 11, 0);
  lv_obj_set_user_data(s_flame3, (void*)35);
  lv_obj_clear_flag(s_flame3, LV_OBJ_FLAG_SCROLLABLE);

  // Animazioni fiamme — rimangono (solo colore/altezza, nessuna transizione schermata)
  lv_anim_t a1;
  lv_anim_init(&a1);
  lv_anim_set_var(&a1, s_flame1);
  lv_anim_set_exec_cb(&a1, flame_color_anim_cb);
  lv_anim_set_values(&a1, 0, 100);
  lv_anim_set_time(&a1, 400);
  lv_anim_set_playback_time(&a1, 400);
  lv_anim_set_repeat_count(&a1, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a1);

  lv_anim_t a2;
  lv_anim_init(&a2);
  lv_anim_set_var(&a2, s_flame2);
  lv_anim_set_exec_cb(&a2, flame_color_anim_cb);
  lv_anim_set_values(&a2, 0, 100);
  lv_anim_set_time(&a2, 350);
  lv_anim_set_playback_time(&a2, 350);
  lv_anim_set_repeat_count(&a2, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_delay(&a2, 130);
  lv_anim_start(&a2);

  lv_anim_t a3;
  lv_anim_init(&a3);
  lv_anim_set_var(&a3, s_flame3);
  lv_anim_set_exec_cb(&a3, flame_color_anim_cb);
  lv_anim_set_values(&a3, 0, 100);
  lv_anim_set_time(&a3, 460);
  lv_anim_set_playback_time(&a3, 460);
  lv_anim_set_repeat_count(&a3, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_delay(&a3, 60);
  lv_anim_start(&a3);

  // ── Barra progresso ──
  lv_obj_t* bar_cont = lv_obj_create(s_scr);
  lv_obj_set_pos(bar_cont, 40, 186);
  lv_obj_set_size(bar_cont, 400, 16);
  lv_obj_set_style_bg_color(bar_cont, SPLASH_BAR_BG, 0);
  lv_obj_set_style_bg_opa(bar_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(bar_cont, lv_color_make(0x30, 0x30, 0x48), 0);
  lv_obj_set_style_border_width(bar_cont, 1, 0);
  lv_obj_set_style_radius(bar_cont, 8, 0);
  lv_obj_set_style_pad_all(bar_cont, 0, 0);
  lv_obj_clear_flag(bar_cont, LV_OBJ_FLAG_SCROLLABLE);

  s_bar = lv_bar_create(bar_cont);
  lv_obj_set_pos(s_bar, 0, 0);
  lv_obj_set_size(s_bar, 400, 16);
  lv_bar_set_range(s_bar, 0, 100);
  // FIX: LV_ANIM_OFF — evita timer LVGL in conflitto durante transizione
  lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_bar, lv_color_make(0x00, 0x00, 0x00), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(s_bar, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_bar, SPLASH_ORANGE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(s_bar, SPLASH_YELLOW, LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(s_bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_bar, 8, LV_PART_INDICATOR);

  // Label messaggio
  s_bar_lbl = lv_label_create(s_scr);
  lv_label_set_text(s_bar_lbl, "Inizializzazione...");
  lv_obj_set_style_text_font(s_bar_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_bar_lbl, SPLASH_GRAY, 0);
  lv_obj_set_pos(s_bar_lbl, 40, 206);
  lv_obj_set_width(s_bar_lbl, 320);

  // Label percentuale
  s_pct_lbl = lv_label_create(s_scr);
  lv_label_set_text(s_pct_lbl, "0%");
  lv_obj_set_style_text_font(s_pct_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_pct_lbl, SPLASH_ORANGE, 0);
  lv_obj_set_pos(s_pct_lbl, 400, 206);
  lv_obj_set_width(s_pct_lbl, 40);
  lv_obj_set_style_text_align(s_pct_lbl, LV_TEXT_ALIGN_RIGHT, 0);

  // Versione
  lv_obj_t* ver = lv_label_create(s_scr);
  lv_label_set_text(ver, "v2.0");
  lv_obj_set_style_text_font(ver, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(ver, lv_color_make(0x30, 0x30, 0x45), 0);
  lv_obj_set_pos(ver, 430, 258);

  // Linea separatore
  lv_obj_t* sep = lv_obj_create(s_scr);
  lv_obj_set_pos(sep, 40, 178);
  lv_obj_set_size(sep, 400, 1);
  lv_obj_set_style_bg_color(sep, lv_color_make(0x30, 0x30, 0x48), 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
}

// ================================================================
//  splash_init() — chiama da setup() dopo display_init()
// ================================================================
inline void splash_init() {
  s_scr = lv_obj_create(nullptr);
  lv_obj_set_size(s_scr, LCD_H_RES, LCD_V_RES);
  lv_obj_set_style_bg_color(s_scr, SPLASH_BG, 0);
  lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

  // Titolo
  lv_obj_t* title = lv_label_create(s_scr);
  lv_label_set_text(title, "FORNO PIZZA");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title, SPLASH_ORANGE, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  splash_build_graphics(s_scr);

  lv_scr_load(s_scr);

  Serial.println("[SPLASH] Schermata avvio inizializzata");
}

// ================================================================
//  splash_set_progress(0..100, messaggio)
// ================================================================
inline void splash_set_progress(int pct, const char* msg) {
  if (!s_bar || !s_bar_lbl || !s_pct_lbl) return;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;

  // FIX: LV_ANIM_OFF — nessun timer interpolazione in accumulo
  lv_bar_set_value(s_bar, pct, LV_ANIM_OFF);

  if (msg) lv_label_set_text(s_bar_lbl, msg);

  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_label_set_text(s_pct_lbl, buf);

  // Pump LVGL per aggiornare il display durante il setup()
  // (Task_LVGL non è ancora partito — dobbiamo chiamare manualmente)
  for (int i = 0; i < 5; i++) {
    lv_tick_inc(5);
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ================================================================
//  splash_finish() — transizione alla schermata MAIN
//
//  FIX: rimossa lv_scr_load_anim — usare lv_scr_load istantaneo.
//  MOTIVO: lv_scr_load_anim con delay > 0 causa un race condition:
//    - Task_LVGL viene avviato subito dopo splash_finish()
//    - Task_LVGL chiama ui_refresh() ogni 100ms
//    - Se la transizione è ancora in corso, LVGL accede a widget
//      della schermata MAIN che non è ancora attiva → crash
//  Con lv_scr_load() la schermata è attiva immediatamente al primo
//  ciclo di Task_LVGL — nessun race condition possibile.
// ================================================================
inline void splash_finish(lv_obj_t* main_screen) {
  if (!s_scr) return;

  splash_set_progress(100, "Pronto!");

  // Breve pausa visiva (300ms) senza animazione
  vTaskDelay(pdMS_TO_TICKS(300));

  // FIX: lv_scr_load istantaneo — no delay, no race condition
  lv_scr_load(main_screen);

  // Elimina la schermata splash per liberare memoria LVGL
  lv_obj_del(s_scr);
  s_scr        = nullptr;
  s_bar        = nullptr;
  s_bar_lbl    = nullptr;
  s_pct_lbl    = nullptr;
  s_flame1     = nullptr;
  s_flame2     = nullptr;
  s_flame3     = nullptr;
  s_splash_done = true;

  Serial.println("[SPLASH] Transizione a MAIN completata");
}

// ================================================================
//  splash_is_done()
// ================================================================
inline bool splash_is_done() {
  return s_splash_done;
}
