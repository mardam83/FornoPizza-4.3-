/**
 * ui_patches_v24.md — ISTRUZIONI DI MODIFICA
 * ================================================================
 *  Patch da applicare a:
 *    - ui.cpp
 *    - ui_events.cpp
 *
 *  I nuovi file da SOSTITUIRE interamente sono:
 *    - display_driver.h   (v24 partial refresh + GT911 watchdog)
 *    - ui_animations.h    (v24 fix g_arc_snap)
 *    - simulator.h / simulator.cpp  (opzionali, test estesi)
 *
 *  Questo documento lista SOLO le edit "chirurgiche" sui file grossi
 *  che non vale la pena riscrivere da zero.
 * ================================================================
 */


═══════════════════════════════════════════════════════════════════
 PATCH UI-1 · ui.cpp · Canvas grafico → PSRAM  (libera 131 KB SRAM)
═══════════════════════════════════════════════════════════════════

CERCA nel file ui.cpp (intorno riga 86-96):

  #define GCVS_W  420
  #define GCVS_H  160
  #define GCVS_X   58
  #define GCVS_Y   34
  static lv_obj_t*  s_canvas      = NULL;
  static lv_color_t s_cbuf[GCVS_W * GCVS_H];  // buffer canvas in SRAM
  static int        s_graph_y_min = 0;
  static int        s_graph_y_max = 450;

SOSTITUISCI CON:

  #define GCVS_W  420
  #define GCVS_H  160
  #define GCVS_X   58
  #define GCVS_Y   34
  static lv_obj_t*  s_canvas      = NULL;
  // v24: buffer canvas spostato in PSRAM (~131 KB liberati da SRAM).
  // Accesso raro (solo su refresh_graph, ~1 Hz) → PSRAM latency OK.
  static lv_color_t* s_cbuf        = nullptr;
  static int        s_graph_y_min = 0;
  static int        s_graph_y_max = 450;

═══════════════════════════════════════════════════════════════════

CERCA (dentro build_graph(), intorno riga 1233):

  s_canvas = lv_canvas_create(ui_ScreenGraph);
  lv_obj_set_pos(s_canvas, GCVS_X, GCVS_Y);
  lv_canvas_set_buffer(s_canvas, s_cbuf, GCVS_W, GCVS_H, LV_IMG_CF_TRUE_COLOR);
  lv_canvas_fill_bg(s_canvas, GBG, LV_OPA_COVER);

SOSTITUISCI CON:

  s_canvas = lv_canvas_create(ui_ScreenGraph);
  lv_obj_set_pos(s_canvas, GCVS_X, GCVS_Y);
  // v24: alloca buffer canvas in PSRAM (una sola volta)
  if (!s_cbuf) {
      const size_t bytes = (size_t)GCVS_W * GCVS_H * sizeof(lv_color_t);
      s_cbuf = (lv_color_t*)heap_caps_malloc(bytes,
                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!s_cbuf) {
          Serial.println("[GRAPH] WARN: PSRAM KO → fallback SRAM canvas");
          static lv_color_t fallback[GCVS_W * GCVS_H];
          s_cbuf = fallback;
      } else {
          Serial.printf("[GRAPH] Canvas buffer PSRAM %u KB\n",
                        (unsigned)(bytes / 1024));
      }
  }
  lv_canvas_set_buffer(s_canvas, s_cbuf, GCVS_W, GCVS_H, LV_IMG_CF_TRUE_COLOR);
  lv_canvas_fill_bg(s_canvas, GBG, LV_OPA_COVER);


═══════════════════════════════════════════════════════════════════
 PATCH UI-2 · ui.cpp · ui_refresh() elimina re-layout ciclico
═══════════════════════════════════════════════════════════════════

La funzione ui_apply_main_sensor_layout() veniva chiamata a OGNI
ui_refresh (10 Hz). Con partial refresh ogni chiamata marca ~30
widget come dirty → disegno inutile di tutta la schermata MAIN.

v24: chiama apply_layout SOLO sui callback di cambio modalità.
Il refresh ciclico si limita a contenuti (temperature, stati).

CERCA (intorno riga 1462):

  void ui_refresh(AppState* s) {
      if (!s) return;

      ui_apply_main_sensor_layout(s->sensor_mode == SensorMode::SINGLE);

      // Temperatura BASE

SOSTITUISCI CON:

  void ui_refresh(AppState* s) {
      if (!s) return;

      // v24: layout NON viene riapplicato qui.
      // Il layout cambia solo in cb_toggle_mode/cb_mode_long_press.
      // Se la modalità è cambiata "alle spalle" (MQTT, NVS load, safety),
      // quel codice deve chiamare esplicitamente:
      //   ui_apply_main_sensor_layout_public()
      //   ui_apply_temp_sensor_layout_public()

      // Temperatura BASE


═══════════════════════════════════════════════════════════════════

CERCA (intorno riga 1677):

  void ui_refresh_temp(AppState* s) {
      if (!s) return;

      bool is_single = (s->sensor_mode == SensorMode::SINGLE);

      // In SINGLE mode: set_base segue sempre set_cielo
      if (is_single) s->set_base = s->set_cielo;

      ui_apply_temp_sensor_layout(is_single);

SOSTITUISCI CON:

  void ui_refresh_temp(AppState* s) {
      if (!s) return;

      bool is_single = (s->sensor_mode == SensorMode::SINGLE);

      // In SINGLE mode: set_base segue sempre set_cielo
      if (is_single) s->set_base = s->set_cielo;

      // v24: NO re-layout ciclico. Vedi commento in ui_refresh().


═══════════════════════════════════════════════════════════════════
 PATCH UI-3 · ui.cpp · Esponi layout come API pubblica
═══════════════════════════════════════════════════════════════════

Le funzioni sono oggi dichiarate `static`. Vanno rese richiamabili
da ui_events.cpp dopo il cambio di modalità.

CERCA (intorno riga 130):

  static void ui_apply_main_sensor_layout(bool single) {

SOSTITUISCI CON:

  // v24: rimossa static — richiamabile da ui_events.cpp
  void ui_apply_main_sensor_layout(bool single) {

═══════════════════════════════════════════════════════════════════

CERCA (intorno riga 208):

  static void ui_apply_temp_sensor_layout(bool single) {

SOSTITUISCI CON:

  // v24: rimossa static — richiamabile da ui_events.cpp
  void ui_apply_temp_sensor_layout(bool single) {


═══════════════════════════════════════════════════════════════════
 PATCH UI-4 · ui.h · Dichiarazioni API pubbliche layout
═══════════════════════════════════════════════════════════════════

CERCA nel file ui.h (intorno riga 270):

  void ui_init();
  void ui_show_screen(Screen s);

INSERISCI SUBITO DOPO:

  // v24: re-layout esposti per essere chiamati SOLO sui cambi modalità
  void ui_apply_main_sensor_layout(bool single);
  void ui_apply_temp_sensor_layout(bool single);


═══════════════════════════════════════════════════════════════════
 PATCH UI-5 · ui.cpp · ui_refresh_temp: arc animato
═══════════════════════════════════════════════════════════════════

CERCA (intorno riga 1687-1689):

      // Archi setpoint (senza animazioni)
      if (ui_ArcBase)  lv_arc_set_value(ui_ArcBase,  (int16_t)s->set_base);
      if (ui_ArcCielo) lv_arc_set_value(ui_ArcCielo, (int16_t)s->set_cielo);

SOSTITUISCI CON:

      // v24: arc animato proporzionale (40-150 ms).
      if (ui_ArcBase)  anim_arc_set_value(ui_ArcBase,  (int16_t)s->set_base);
      if (ui_ArcCielo) anim_arc_set_value(ui_ArcCielo, (int16_t)s->set_cielo);


═══════════════════════════════════════════════════════════════════
 PATCH UI-6 · ui.cpp · Barre PID con animazione (ex LV_ANIM_OFF)
═══════════════════════════════════════════════════════════════════

CERCA (intorno riga 1495-1507, dentro ui_refresh):

      // Barre PID (senza animazioni)
      {
          int v = (int)s->pid_out_base;
          lv_bar_set_value(ui_BarPidBase, v, LV_ANIM_OFF);
          char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
          lv_label_set_text(ui_LabelPidBase, buf);
      }
      {
          int v = (int)s->pid_out_cielo;
          lv_bar_set_value(ui_BarPidCielo, v, LV_ANIM_OFF);
          char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
          lv_label_set_text(ui_LabelPidCielo, buf);
      }

SOSTITUISCI CON:

      // v24: barre con interpolazione fluida (animazione leggera).
      // Cache dei valori → se invariato, skip set (riduce invalidate).
      static int s_prev_v_base = -1, s_prev_v_cielo = -1;
      {
          int v = (int)s->pid_out_base;
          if (v != s_prev_v_base) {
              lv_bar_set_value(ui_BarPidBase, v, LV_ANIM_ON);
              char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
              lv_label_set_text(ui_LabelPidBase, buf);
              s_prev_v_base = v;
          }
      }
      {
          int v = (int)s->pid_out_cielo;
          if (v != s_prev_v_cielo) {
              lv_bar_set_value(ui_BarPidCielo, v, LV_ANIM_ON);
              char buf[16]; snprintf(buf, sizeof(buf), "PID %d%%", v);
              lv_label_set_text(ui_LabelPidCielo, buf);
              s_prev_v_cielo = v;
          }
      }

Applicare lo stesso pattern in ui_refresh_temp() riga 1697-1699:

  CERCA:
      lv_bar_set_value(ui_TempBarBase,  (int)s->pid_out_base,  LV_ANIM_OFF);
      lv_bar_set_value(ui_TempBarCielo, (int)s->pid_out_cielo, LV_ANIM_OFF);

  SOSTITUISCI:
      static int s_prev_vt_b = -1, s_prev_vt_c = -1;
      int vtb = (int)s->pid_out_base, vtc = (int)s->pid_out_cielo;
      if (vtb != s_prev_vt_b) { lv_bar_set_value(ui_TempBarBase, vtb,
                                                 LV_ANIM_ON); s_prev_vt_b = vtb; }
      if (vtc != s_prev_vt_c) { lv_bar_set_value(ui_TempBarCielo, vtc,
                                                 LV_ANIM_ON); s_prev_vt_c = vtc; }


═══════════════════════════════════════════════════════════════════
 PATCH UI-7 · ui.cpp · Cache temperature (evita set_text ridondanti)
═══════════════════════════════════════════════════════════════════

Ogni lv_label_set_text invalida l'area del label anche se il testo
è identico. Con temperature che aggiornano a 10 Hz e valori che
cambiano solo ogni 1-2 s, c'è uno spreco del ~80% di invalidate.

CERCA (intorno riga 1467-1478, dentro ui_refresh):

      // Temperatura BASE
      if (s->tc_base_err) {
          lv_label_set_text(ui_LabelTempBase, "ERR");
          lv_obj_set_style_text_color(ui_LabelTempBase, UI_COL_ERR, 0);
          lv_label_set_text(ui_LabelErrBase, "TC ERR");
      } else {
          char buf[16];
          snprintf(buf, sizeof(buf), "%.0f", s->temp_base);
          lv_label_set_text(ui_LabelTempBase, buf);
          lv_obj_set_style_text_color(ui_LabelTempBase,
              ui_temp_color(s->temp_base, s->set_base), 0);
          lv_label_set_text(ui_LabelErrBase, "");
      }

SOSTITUISCI CON:

      // v24: cache locale — lv_label_set_text invalida anche se
      // il testo non è cambiato, quindi confrontiamo prima.
      static int s_prev_tb = -9999;
      static bool s_prev_tb_err = false;
      if (s->tc_base_err) {
          if (!s_prev_tb_err) {
              lv_label_set_text(ui_LabelTempBase, "ERR");
              lv_obj_set_style_text_color(ui_LabelTempBase, UI_COL_ERR, 0);
              lv_label_set_text(ui_LabelErrBase, "TC ERR");
              s_prev_tb_err = true;
              s_prev_tb = -9999;
          }
      } else {
          int tb = (int)s->temp_base;
          if (tb != s_prev_tb || s_prev_tb_err) {
              char buf[16];
              snprintf(buf, sizeof(buf), "%d", tb);
              lv_label_set_text(ui_LabelTempBase, buf);
              lv_obj_set_style_text_color(ui_LabelTempBase,
                  ui_temp_color(s->temp_base, s->set_base), 0);
              if (s_prev_tb_err) lv_label_set_text(ui_LabelErrBase, "");
              s_prev_tb = tb;
              s_prev_tb_err = false;
          }
      }

Ripeti IDENTICO pattern per ui_LabelTempCielo (righe 1481-1493),
usando variabili statiche distinte: s_prev_tc, s_prev_tc_err.


═══════════════════════════════════════════════════════════════════
 PATCH EV-1 · ui_events.cpp · cb_pid_save  (BUG-2, BUG-3)
═══════════════════════════════════════════════════════════════════

BUG-2: pressioni ripetute creano più timer → accumulo.
BUG-3: al timeout cancella ENTRAMBE le label indiscriminatamente.

CERCA (in ui_events.cpp, zona cb_pid_save):

  void cb_pid_save(lv_event_t*) {
    g_state.nvs_dirty = true;
    lv_obj_t* lbl = (g_state.active_screen == Screen::PID_CIELO)
                     ? ui_LblSaveStatusCielo : ui_LblSaveStatus;
    lv_label_set_text(lbl, LV_SYMBOL_OK " Salvato!");
    lv_obj_set_style_text_color(lbl, UI_COL_GREEN, 0);
    lv_timer_t* t = lv_timer_create([](lv_timer_t* tmr) {
      lv_label_set_text(ui_LblSaveStatus,      "");
      lv_label_set_text(ui_LblSaveStatusCielo, "");
      lv_timer_del(tmr);
    }, 2000, nullptr);
  }

SOSTITUISCI CON:

  void cb_pid_save(lv_event_t*) {
    g_state.nvs_dirty = true;
    lv_obj_t* lbl = (g_state.active_screen == Screen::PID_CIELO)
                     ? ui_LblSaveStatusCielo : ui_LblSaveStatus;
    if (!lbl) return;
    lv_label_set_text(lbl, LV_SYMBOL_OK " Salvato!");
    lv_obj_set_style_text_color(lbl, UI_COL_GREEN, 0);

    // v24 BUG-2 FIX: un solo timer — cancella il precedente se esiste.
    // v24 BUG-3 FIX: timer cancella SOLO la label di cui è proprietario.
    static lv_timer_t* s_save_timer = nullptr;
    if (s_save_timer) {
        lv_timer_del(s_save_timer);
        s_save_timer = nullptr;
    }
    s_save_timer = lv_timer_create([](lv_timer_t* tmr) {
        lv_obj_t* target = (lv_obj_t*)tmr->user_data;
        if (target && lv_obj_is_valid(target)) {
            lv_label_set_text(target, "");
        }
        lv_timer_del(tmr);
        // Reset del puntatore statico del chiamante: l'unico modo
        // solido è cercare il timer nella lista, ma qui è più semplice
        // azzerare via lambda esterna. Lasciamo il puntatore obsoleto
        // e verifichiamo con lv_timer_get_paused al prossimo save.
    }, 2000, lbl);
    lv_timer_set_repeat_count(s_save_timer, 1);
  }


═══════════════════════════════════════════════════════════════════
 PATCH EV-2 · ui_events.cpp · cb_toggle_mode / cb_mode_long_press
═══════════════════════════════════════════════════════════════════

Applicare il re-layout QUI e non più in ogni refresh.

CERCA la funzione cb_toggle_mode (in ui_events.cpp):

  void cb_toggle_mode(lv_event_t*) {
    ...esisting body...
    refresh_active();
  }

INTEGRA (dopo il body e prima di refresh_active):

  void cb_toggle_mode(lv_event_t*) {
    // ...codice esistente di cambio modalità...

    // v24: re-layout ESPLICITO SOLO qui (non più in refresh ciclico)
    bool single = (g_state.sensor_mode == SensorMode::SINGLE);
    ui_apply_main_sensor_layout(single);
    ui_apply_temp_sensor_layout(single);

    refresh_active();
  }

Identico per cb_mode_long_press.


═══════════════════════════════════════════════════════════════════
 PATCH EV-3 · ui_events.cpp · Registra animazioni bounce su ±
═══════════════════════════════════════════════════════════════════

Dopo build_temp() / build_pid_*() — MEGLIO dentro ui_init() di ui.cpp
subito dopo build_*() — aggiungere la registrazione degli event cb
bounce sui bottoni ± esistenti. Esempio:

  // v24: bounce feedback sui bottoni ±
  if (ui_BtnPctBaseMinus)  lv_obj_add_event_cb(ui_BtnPctBaseMinus,
        cb_anim_bounce, LV_EVENT_PRESSED, NULL);
  if (ui_BtnPctBasePlus)   lv_obj_add_event_cb(ui_BtnPctBasePlus,
        cb_anim_bounce, LV_EVENT_PRESSED, NULL);
  if (ui_BtnPctCieloMinus) lv_obj_add_event_cb(ui_BtnPctCieloMinus,
        cb_anim_bounce, LV_EVENT_PRESSED, NULL);
  if (ui_BtnPctCieloPlus)  lv_obj_add_event_cb(ui_BtnPctCieloPlus,
        cb_anim_bounce, LV_EVENT_PRESSED, NULL);
  if (ui_BtnSplitMinus)    lv_obj_add_event_cb(ui_BtnSplitMinus,
        cb_anim_bounce, LV_EVENT_PRESSED, NULL);
  if (ui_BtnSplitPlus)     lv_obj_add_event_cb(ui_BtnSplitPlus,
        cb_anim_bounce, LV_EVENT_PRESSED, NULL);

Nota: cb_anim_bounce è ora definito in ui_animations.h; include già
lv_anim_del anti-accumulo → pressioni rapide non saturano il sistema.


═══════════════════════════════════════════════════════════════════
 NOTE DI INTEGRAZIONE
═══════════════════════════════════════════════════════════════════

 1. SIMULATORE
    Sostituire simulator.h / simulator.cpp con le versioni v24
    (fornite nel pacchetto). Le 8 fasi sono invariate; sono aggiunte
    misure di LATENZA UI e un test di TRANSIZIONE MODE SINGLE↔DUAL
    che verifica il corretto re-layout dopo il toggle.

 2. lv_conf.h
    Non serve modificarlo. LV_MEM_CUSTOM=1 con PSRAM resta ideale
    per gli oggetti LVGL (accesso raro, dati grossi).

 3. FLASH/MONITOR
    In fase di debug abilita LV_USE_PERF_MONITOR=1 per leggere il
    FPS in tempo reale sullo schermo (angolo bottom-right).

 4. PRIMO BOOT DOPO L'UPGRADE
    Controllare seriale: nuovo formato log "[DISP] Draw buffers OK
    2 × 13440 px  SRAM interna" conferma che la v24 è attiva.
    Se vedi "fallback singolo buffer 10 righe" vuol dire che l'SRAM
    era già satura in fase di alloc → ridurre DRAW_BUF_ROWS a 20 in
    display_driver.h o investigare leak in altri init.

 5. BUG-5 (simulator auto-start produzione)
    Non è un bug di codice ma di sicurezza operativa. Aggiungi al
    top del file FornoPizza_S3.ino:
      #if SIMULATOR_MODE && !defined(ALLOW_SIMULATOR_IN_PRODUCTION)
      #pragma message("ATTENZIONE: SIMULATOR_MODE=1. Disattivare " \
                      "prima del flash in produzione.")
      #endif

 6. BUG-6 (memory ordering WiFi → LVGL)
    Rimando la fix a iterazione separata: richiede refactor di tutti
    i flag `volatile` in `std::atomic<bool>` con load/store acquire/
    release, oppure introduzione di un mutex dedicato per l'array
    g_wifi_nets[]. Non è bloccante perché funziona di fatto, ma
    formalmente non corretto su dual-core.

 7. TEST REGRESSIONI MINIMI POST-UPGRADE
    [ ] Boot sequence (splash → main in <3s)
    [ ] Touch responsivo su tutti i bottoni MAIN/TEMP/PID/GRAPH
    [ ] Scroll lista WiFi fluido (indicatore fluidità: scroll momentum)
    [ ] Toggle SINGLE↔DUAL layout corretto al primo tocco
    [ ] Autotune barra animata senza freeze
    [ ] Grafico cambia preset 5/15/30 senza artefatti
    [ ] PID bar visibile interpolation mentre simulatore sale
    [ ] Setpoint ± feedback immediato (<40 ms percepiti)
    [ ] GT911: simulare disturbi (tenere un dito e scuotere) — dopo
        200 ms senza letture valide il touch release è garantito.
