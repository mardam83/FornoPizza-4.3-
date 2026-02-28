/**
 * splash_screen.h — Schermata di avvio animata
 * ================================================================
 * Forno Pizza Controller — ESP32-S3
 *
 * Sequenza animazione (totale ~3.5 secondi):
 *   0ms     → fade-in sfondo + titolo "FORNO PIZZA"
 *   400ms   → comparsa icona forno (disegnata con LVGL primitives)
 *   700ms   → fiamma animata (oscillazione + colore)
 *   1000ms  → comparsa barra di caricamento
 *   1000ms+ → barra avanza mentre il setup procede (progress reale)
 *   fine    → fade-out + transizione alla schermata MAIN
 *
 * USO:
 *   1. Chiama splash_init()   dopo display_init() in setup()
 *   2. Chiama splash_set_progress(0..100, "messaggio") durante il setup
 *   3. Chiama splash_finish() per avviare il fade-out verso MAIN
 *   4. Nel Task_LVGL chiama splash_is_done() per sapere quando rimuoverla
 * ================================================================
 */

#pragma once
#include <lvgl.h>
#include "hardware.h"

// ================================================================
//  OGGETTI SPLASH
// ================================================================
static lv_obj_t* s_scr        = nullptr;  // schermata splash
static lv_obj_t* s_bar        = nullptr;  // barra progresso
static lv_obj_t* s_bar_lbl    = nullptr;  // testo sotto la barra
static lv_obj_t* s_pct_lbl    = nullptr;  // percentuale
static lv_obj_t* s_flame1     = nullptr;  // fiamma sinistra
static lv_obj_t* s_flame2     = nullptr;  // fiamma destra
static lv_obj_t* s_flame3     = nullptr;  // fiamma centro
static lv_obj_t* s_door       = nullptr;  // porta forno
static lv_obj_t* s_subtitle   = nullptr;  // sottotitolo "Controller PID"
static lv_anim_t s_flame_anim;

static volatile bool s_splash_done    = false;
static volatile bool s_splash_fadeout = false;

// ================================================================
//  COLORI SPLASH
// ================================================================
#define SPLASH_BG        lv_color_make(0x08, 0x08, 0x14)
#define SPLASH_ORANGE    lv_color_make(0xFF, 0x6B, 0x00)
#define SPLASH_ORANGE2   lv_color_make(0xFF, 0x9E, 0x40)
#define SPLASH_RED       lv_color_make(0xFF, 0x22, 0x00)
#define SPLASH_YELLOW    lv_color_make(0xFF, 0xE0, 0x00)
#define SPLASH_WHITE     lv_color_make(0xFF, 0xFF, 0xFF)
#define SPLASH_GRAY      lv_color_make(0x40, 0x40, 0x55)
#define SPLASH_DARKGRAY  lv_color_make(0x20, 0x20, 0x30)
#define SPLASH_GREEN     lv_color_make(0x00, 0xE6, 0x76)
#define SPLASH_BAR_BG    lv_color_make(0x18, 0x18, 0x28)

// ================================================================
//  CALLBACK ANIMAZIONE FIAMMA — oscillazione colore/opacità
// ================================================================
static void flame_color_anim_cb(void* obj, int32_t v) {
  // v va da 0 a 100 e ritorna → simula pulsazione
  lv_obj_t* o = (lv_obj_t*)obj;
  uint8_t r = 255;
  uint8_t g = (uint8_t)(60  + (v * 100 / 100));   // varia 60..160
  uint8_t b = 0;
  lv_obj_set_style_bg_color(o, lv_color_make(r, g, b), 0);
  // Opacità pulsante
  lv_obj_set_style_bg_opa(o, (lv_opa_t)(180 + v * 75 / 100), 0);
}

static void flame_size_anim_cb(void* obj, int32_t v) {
  lv_obj_t* o = (lv_obj_t*)obj;
  // v: 0..10 → variazione altezza ±5px
  int base_h = (int)(uintptr_t)lv_obj_get_user_data(o);
  lv_obj_set_height(o, base_h + v - 5);
}

// ================================================================
//  DISEGNA FORNO — primitives LVGL
//
//  Layout centrato a x=240, y=40..145 (105px totale)
//
//  Struttura:
//    corpo forno  : rettangolo arrotondato 160×80 px, centrato
//    sportello    : rettangolo 100×55 px con bordo arancio + vetro
//    resistenze   : 2 linee orizzontali arancioni (base/cielo)
//    piedi        : 2 rettangoli piccoli
//    fiamme       : 3 forme appuntite sopra il forno
// ================================================================
static void build_oven(lv_obj_t* parent) {
  // ── Corpo forno ──
  lv_obj_t* body = lv_obj_create(parent);
  lv_obj_set_pos(body, 80, 70);
  lv_obj_set_size(body, 320, 90);
  lv_obj_set_style_bg_color(body, lv_color_make(0x28, 0x28, 0x38), 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(body, SPLASH_GRAY, 0);
  lv_obj_set_style_border_width(body, 3, 0);
  lv_obj_set_style_radius(body, 12, 0);
  lv_obj_set_style_shadow_width(body, 20, 0);
  lv_obj_set_style_shadow_color(body, SPLASH_ORANGE, 0);
  lv_obj_set_style_shadow_opa(body, 80, 0);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  // ── Sportello (vetro) ──
  s_door = lv_obj_create(parent);
  lv_obj_set_pos(s_door, 150, 76);
  lv_obj_set_size(s_door, 180, 78);
  lv_obj_set_style_bg_color(s_door, lv_color_make(0x10, 0x14, 0x22), 0);
  lv_obj_set_style_bg_opa(s_door, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(s_door, SPLASH_ORANGE, 0);
  lv_obj_set_style_border_width(s_door, 3, 0);
  lv_obj_set_style_radius(s_door, 8, 0);
  lv_obj_clear_flag(s_door, LV_OBJ_FLAG_SCROLLABLE);

  // Riflesso vetro (diagonale simulata con rettangolo sottile)
  lv_obj_t* glass = lv_obj_create(s_door);
  lv_obj_set_pos(glass, 8, 6);
  lv_obj_set_size(glass, 40, 6);
  lv_obj_set_style_bg_color(glass, lv_color_make(0x80, 0x90, 0xFF), 0);
  lv_obj_set_style_bg_opa(glass, 60, 0);
  lv_obj_set_style_border_width(glass, 0, 0);
  lv_obj_set_style_radius(glass, 3, 0);
  lv_obj_clear_flag(glass, LV_OBJ_FLAG_SCROLLABLE);

  // ── Resistenza CIELO (linea rossa in alto) ──
  lv_obj_t* res_cielo = lv_obj_create(parent);
  lv_obj_set_pos(res_cielo, 90, 78);
  lv_obj_set_size(res_cielo, 50, 4);
  lv_obj_set_style_bg_color(res_cielo, lv_color_make(0xFF, 0x30, 0x30), 0);
  lv_obj_set_style_bg_opa(res_cielo, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(res_cielo, 0, 0);
  lv_obj_set_style_radius(res_cielo, 2, 0);
  lv_obj_set_style_shadow_width(res_cielo, 8, 0);
  lv_obj_set_style_shadow_color(res_cielo, lv_color_make(0xFF, 0x30, 0x30), 0);
  lv_obj_clear_flag(res_cielo, LV_OBJ_FLAG_SCROLLABLE);

  // ── Resistenza BASE (linea arancio in basso) ──
  lv_obj_t* res_base = lv_obj_create(parent);
  lv_obj_set_pos(res_base, 90, 138);
  lv_obj_set_size(res_base, 50, 4);
  lv_obj_set_style_bg_color(res_base, SPLASH_ORANGE, 0);
  lv_obj_set_style_bg_opa(res_base, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(res_base, 0, 0);
  lv_obj_set_style_radius(res_base, 2, 0);
  lv_obj_set_style_shadow_width(res_base, 8, 0);
  lv_obj_set_style_shadow_color(res_base, SPLASH_ORANGE, 0);
  lv_obj_clear_flag(res_base, LV_OBJ_FLAG_SCROLLABLE);

  // ── Manopola temperatura ──
  lv_obj_t* knob = lv_obj_create(parent);
  lv_obj_set_pos(knob, 360, 95);
  lv_obj_set_size(knob, 28, 28);
  lv_obj_set_style_bg_color(knob, lv_color_make(0x38, 0x38, 0x50), 0);
  lv_obj_set_style_border_color(knob, SPLASH_ORANGE, 0);
  lv_obj_set_style_border_width(knob, 2, 0);
  lv_obj_set_style_radius(knob, 14, 0);
  lv_obj_clear_flag(knob, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* knob_dot = lv_obj_create(parent);
  lv_obj_set_pos(knob_dot, 371, 101);
  lv_obj_set_size(knob_dot, 6, 6);
  lv_obj_set_style_bg_color(knob_dot, SPLASH_ORANGE, 0);
  lv_obj_set_style_border_width(knob_dot, 0, 0);
  lv_obj_set_style_radius(knob_dot, 3, 0);
  lv_obj_clear_flag(knob_dot, LV_OBJ_FLAG_SCROLLABLE);

  // ── Piedi forno ──
  for (int i = 0; i < 2; i++) {
    lv_obj_t* foot = lv_obj_create(parent);
    lv_obj_set_pos(foot, 120 + i * 220, 158);
    lv_obj_set_size(foot, 20, 10);
    lv_obj_set_style_bg_color(foot, SPLASH_DARKGRAY, 0);
    lv_obj_set_style_border_width(foot, 0, 0);
    lv_obj_set_style_radius(foot, 3, 0);
    lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);
  }

  // ── Fiamme (triangoli approssimati con rettangoli a raggio variabile) ──
  // Fiamma sinistra
  s_flame1 = lv_obj_create(parent);
  lv_obj_set_pos(s_flame1, 170, 38);
  lv_obj_set_size(s_flame1, 22, 35);
  lv_obj_set_style_bg_color(s_flame1, SPLASH_RED, 0);
  lv_obj_set_style_bg_opa(s_flame1, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_flame1, 0, 0);
  lv_obj_set_style_radius(s_flame1, 11, 0);
  lv_obj_set_user_data(s_flame1, (void*)35);
  lv_obj_clear_flag(s_flame1, LV_OBJ_FLAG_SCROLLABLE);

  // Fiamma centrale (più grande)
  s_flame2 = lv_obj_create(parent);
  lv_obj_set_pos(s_flame2, 228, 28);
  lv_obj_set_size(s_flame2, 28, 45);
  lv_obj_set_style_bg_color(s_flame2, SPLASH_ORANGE, 0);
  lv_obj_set_style_bg_opa(s_flame2, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_flame2, 0, 0);
  lv_obj_set_style_radius(s_flame2, 14, 0);
  lv_obj_set_user_data(s_flame2, (void*)45);
  lv_obj_clear_flag(s_flame2, LV_OBJ_FLAG_SCROLLABLE);

  // Fiamma destra
  s_flame3 = lv_obj_create(parent);
  lv_obj_set_pos(s_flame3, 288, 38);
  lv_obj_set_size(s_flame3, 22, 35);
  lv_obj_set_style_bg_color(s_flame3, SPLASH_YELLOW, 0);
  lv_obj_set_style_bg_opa(s_flame3, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_flame3, 0, 0);
  lv_obj_set_style_radius(s_flame3, 11, 0);
  lv_obj_set_user_data(s_flame3, (void*)35);
  lv_obj_clear_flag(s_flame3, LV_OBJ_FLAG_SCROLLABLE);
}

// ================================================================
//  CALLBACK FADE-OUT COMPLETATO → segna splash come done
// ================================================================
static void splash_fade_done_cb(lv_anim_t* a) {
  s_splash_done = true;
}

// ================================================================
//  splash_init() — chiama da setup() dopo display_init()
// ================================================================
inline void splash_init() {
  // ── Crea schermata ──
  s_scr = lv_obj_create(nullptr);
  lv_obj_set_size(s_scr, LCD_H_RES, LCD_V_RES);
  lv_obj_set_style_bg_color(s_scr, SPLASH_BG, 0);
  lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_scr_load(s_scr);

  // ── Titolo principale ──
  lv_obj_t* title = lv_label_create(s_scr);
  lv_label_set_text(title, "FORNO PIZZA");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(title, SPLASH_ORANGE, 0);
  lv_obj_set_pos(title, 0, 0);
  lv_obj_set_width(title, LCD_H_RES);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  // Animazione fade-in titolo
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, title);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_text_opa);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_time(&a, 600);
  lv_anim_set_delay(&a, 0);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);

  // ── Sottotitolo ──
  s_subtitle = lv_label_create(s_scr);
  lv_label_set_text(s_subtitle, "Controller PID  |  ESP32-S3");
  lv_obj_set_style_text_font(s_subtitle, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_subtitle, SPLASH_GRAY, 0);
  lv_obj_set_pos(s_subtitle, 0, 40);
  lv_obj_set_width(s_subtitle, LCD_H_RES);
  lv_obj_set_style_text_align(s_subtitle, LV_TEXT_ALIGN_CENTER, 0);

  lv_anim_init(&a);
  lv_anim_set_var(&a, s_subtitle);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_text_opa);
  lv_anim_set_values(&a, 0, 255);
  lv_anim_set_time(&a, 500);
  lv_anim_set_delay(&a, 300);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);

  // ── Disegna forno ──
  build_oven(s_scr);

  // ── Animazione fiamme — colore pulsante ──
  // Fiamma 1
  lv_anim_init(&s_flame_anim);
  lv_anim_set_var(&s_flame_anim, s_flame1);
  lv_anim_set_exec_cb(&s_flame_anim, flame_color_anim_cb);
  lv_anim_set_values(&s_flame_anim, 0, 100);
  lv_anim_set_time(&s_flame_anim, 400);
  lv_anim_set_playback_time(&s_flame_anim, 400);
  lv_anim_set_repeat_count(&s_flame_anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_delay(&s_flame_anim, 0);
  lv_anim_start(&s_flame_anim);

  // Fiamma 2 (sfasata di 130ms)
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

  // Fiamma 3 (sfasata di 60ms)
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
  // Contenitore barra
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
  lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_bar, lv_color_make(0x00, 0x00, 0x00), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(s_bar, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_bar, SPLASH_ORANGE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(s_bar, SPLASH_YELLOW, LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(s_bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_bar, 8, LV_PART_INDICATOR);
  lv_obj_set_style_shadow_width(s_bar, 8, LV_PART_INDICATOR);
  lv_obj_set_style_shadow_color(s_bar, SPLASH_ORANGE, LV_PART_INDICATOR);

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

  // ── Versione ──
  lv_obj_t* ver = lv_label_create(s_scr);
  lv_label_set_text(ver, "v2.0");
  lv_obj_set_style_text_font(ver, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(ver, lv_color_make(0x30, 0x30, 0x45), 0);
  lv_obj_set_pos(ver, 430, 258);

  // ── Linea separatore decorativa ──
  lv_obj_t* sep = lv_obj_create(s_scr);
  lv_obj_set_pos(sep, 40, 178);
  lv_obj_set_size(sep, 400, 1);
  lv_obj_set_style_bg_color(sep, lv_color_make(0x30, 0x30, 0x48), 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

  Serial.println("[SPLASH] Schermata avvio inizializzata");
}

// ================================================================
//  splash_set_progress(0..100, messaggio)
//  Chiama durante il setup() per aggiornare la barra in tempo reale
// ================================================================
inline void splash_set_progress(int pct, const char* msg) {
  if (!s_bar || !s_bar_lbl || !s_pct_lbl) return;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;

  lv_bar_set_value(s_bar, pct, LV_ANIM_ON);

  if (msg) lv_label_set_text(s_bar_lbl, msg);

  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_label_set_text(s_pct_lbl, buf);

  // Pump LVGL per aggiornare il display immediatamente
  for (int i = 0; i < 5; i++) {
    lv_tick_inc(5);
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ================================================================
//  splash_finish() — avvia il fade-out e la transizione a MAIN
//  Chiama al termine del setup(), dopo ui_show_screen(Screen::MAIN)
// ================================================================
inline void splash_finish(lv_obj_t* main_screen) {
  if (!s_scr) return;

  // Porta la barra al 100%
  splash_set_progress(100, "Pronto!");
  vTaskDelay(pdMS_TO_TICKS(300));

  // Fade-out della splash screen
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_scr);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
  lv_anim_set_values(&a, 255, 0);
  lv_anim_set_time(&a, 500);
  lv_anim_set_delay(&a, 200);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  lv_anim_set_deleted_cb(&a, splash_fade_done_cb);
  lv_anim_start(&a);

  // Carica schermata MAIN con fade-in in parallelo
  lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_FADE_ON, 600, 400, false);
}

// ================================================================
//  splash_is_done() — ritorna true quando l'animazione è terminata
// ================================================================
inline bool splash_is_done() {
  return s_splash_done;
}
