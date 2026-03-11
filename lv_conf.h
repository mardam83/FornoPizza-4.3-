/**
 * lv_conf.h — LVGL 8.3.x — Forno Pizza S3
 *
 * POSIZIONE: Documents/Arduino/libraries/lv_conf.h
 *            (stesso livello della cartella lvgl/, NON dentro)
 *
 * ════════════════════════════════════════════════════════════════
 *  FIX CRITICI (da ESP32_CYD_LVGL_BugFix_Guide):
 *
 *  [1] LV_MEM_POOL_INCLUDE/ALLOC/FREE — COMMENTATE
 *      Con LV_MEM_CUSTOM=0 definire POOL_ALLOC/FREE crea collisione
 *      di simboli con l'allocatore interno → function pointer NULL
 *      → crash in lv_init(). COMMENTARE tutte e tre.
 *
 *  [2] LV_TXT_COLOR_CMD "#" — virgolette DOPPIE
 *      Virgolette singole → char, non const char* → errore tipo.
 *
 *  [3] LV_ATTRIBUTE_FAST_MEM — VUOTO (non IRAM_ATTR)
 *      Con IRAM_ATTR LVGL forza il codice di rendering in IRAM.
 *      L'IRAM dell'ESP32-S3 è ~384KB ma condivisa con WiFi/BT.
 *      Con i task stack + WiFi l'overflow IRAM causa crash
 *      con EXCVADDR 0x83xxxxxx (Flash non cacheable).
 *      → Lasciare VUOTO: LVGL resta in Flash (DROM), stabile.
 *
 *  [4] LV_MEM_CUSTOM=1 con allocatore PSRAM
 *      Invece di un heap fisso da 64KB in SRAM, LVGL usa
 *      heap_caps_malloc(MALLOC_CAP_SPIRAM) → gli oggetti UI
 *      (widget, label, stili) vanno in PSRAM.
 *      IMPORTANTE: gli oggetti LVGL vengono acceduti solo da
 *      Task_LVGL (Core 0) quindi la latenza PSRAM non è critica.
 *      Questo libera ~64KB di SRAM per stack task e WiFi.
 *
 *  [5] LV_MEM_POOL_* — tutte COMMENTATE (coerente con CUSTOM=1)
 * ════════════════════════════════════════════════════════════════
 */

#if 1
#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>

/*==========================
   Risoluzione e colore
===========================*/
#define LV_HOR_RES_MAX    480
#define LV_VER_RES_MAX    272
#define LV_COLOR_DEPTH    16
// ST7701A SPI: non serve byte-swap (il driver invia in ordine corretto)
// Se i colori appaiono invertiti → cambia a 1
#define LV_COLOR_16_SWAP  0

/*==========================
   Memoria LVGL heap
   FIX [4]: LV_MEM_CUSTOM=1 → alloca oggetti UI in PSRAM
   Libera ~64KB di SRAM interna rispetto a LV_MEM_CUSTOM=0
===========================*/
#define LV_MEM_CUSTOM  1

#if LV_MEM_CUSTOM
  // Usiamo heap_caps_malloc per indirizzare la PSRAM
  #include <esp_heap_caps.h>
  #define LV_MEM_CUSTOM_INCLUDE  <esp_heap_caps.h>
  // Alloca prima in PSRAM, fallback a SRAM se non disponibile
  #define LV_MEM_CUSTOM_ALLOC(size) \
    heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
  #define LV_MEM_CUSTOM_FREE        heap_caps_free
  // realloc: heap_caps_realloc con stessi flag
  #define LV_MEM_CUSTOM_REALLOC(ptr, size) \
    heap_caps_realloc((ptr), (size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#else
  // Modalità allocatore interno (fallback se PSRAM non disponibile)
  #define LV_MEM_SIZE  (64U * 1024U)
  #define LV_MEM_ADR   0
  // FIX [1]: COMMENTATE — causano crash con LV_MEM_CUSTOM=0
  //#define LV_MEM_POOL_INCLUDE ""
  //#define LV_MEM_POOL_ALLOC   lv_mem_alloc
  //#define LV_MEM_POOL_FREE    lv_mem_free
#endif

/*==========================
   Tick & refresh
===========================*/
#define LV_DISP_DEF_REFR_PERIOD   20   // ms refresh display
#define LV_INDEV_DEF_READ_PERIOD  30   // ms polling touch

/*==========================
   Font Montserrat
   Solo i taglie effettivamente usate nel codice (analisi ui.cpp):
     10 → label ±5°C
     12 → status, splash
     14 → DEFAULT
     16 → header panel
     18 → header WiFi/OTA
     22 → setpoint arc (TempSetBase/Cielo)
     28 → bottoni ± (make_pm)
     32 → Kp/Ki/Kd schermate PID
     36 → splash titolo (montserrat_28 usato come sostituto se 36 off)
     48 → temperatura MAIN
===========================*/
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  1
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_18  1
#define LV_FONT_MONTSERRAT_20  0
#define LV_FONT_MONTSERRAT_22  1
#define LV_FONT_MONTSERRAT_24  0
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  1
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  1
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  1
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  1
#define LV_FONT_DEFAULT        &lv_font_montserrat_14

#define LV_FONT_UNSCII_8                0
#define LV_FONT_UNSCII_16               0
#define LV_FONT_SIMSUN_16_CJK           0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0

/*==========================
   Rendering
===========================*/
#define LV_DRAW_COMPLEX         1
#define LV_SHADOW_CACHE_SIZE    0
#define LV_CIRCLE_CACHE_SIZE    4
#define LV_IMG_CACHE_DEF_SIZE   0
#define LV_GRADIENT_MAX_STOPS   2
#define LV_GRAD_CACHE_DEF_SIZE  0

/*==========================
   Animazioni
===========================*/
#define LV_USE_ANIMATION  1

/*==========================
   Widget — solo quelli usati
===========================*/
#define LV_USE_ARC       1
#define LV_USE_BAR       1
#define LV_USE_BTN       1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS    0
#define LV_USE_CHECKBOX  0
#define LV_USE_DROPDOWN  0
#define LV_USE_IMG       1
#define LV_USE_LABEL     1
#define LV_USE_LINE      1
#define LV_USE_ROLLER    0
#define LV_USE_SLIDER    0
#define LV_USE_SWITCH    0
#define LV_USE_TEXTAREA  1
#define LV_USE_TABLE     0
#define LV_USE_CHART     1
#define LV_USE_LED       1
#define LV_USE_METER     0
#define LV_USE_MSGBOX    0
#define LV_USE_SPINBOX   0
#define LV_USE_SPINNER   0
#define LV_USE_TABVIEW   0
#define LV_USE_TILEVIEW  0
#define LV_USE_WIN       0
#define LV_USE_SPAN      0
#define LV_USE_CALENDAR  0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN    0
#define LV_USE_KEYBOARD  1
#define LV_USE_LIST      1
#define LV_USE_MENU      1

/*==========================
   Tema scuro
===========================*/
#define LV_USE_THEME_DEFAULT   1
#define LV_THEME_DEFAULT_DARK  1
#define LV_USE_THEME_BASIC     0
#define LV_USE_THEME_MONO      0

/*==========================
   Testo
===========================*/
#define LV_TXT_ENC            LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS    " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
// FIX [2]: virgolette DOPPIE — era '#' (char, non string → crash)
#define LV_TXT_COLOR_CMD      "#"
#define LV_USE_BIDI           0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==========================
   Log — solo WARN in produzione
===========================*/
#define LV_USE_LOG             1
#define LV_LOG_LEVEL           LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF          1
#define LV_LOG_TRACE_MEM       0
#define LV_LOG_TRACE_TIMER     0
#define LV_LOG_TRACE_INDEV     0
#define LV_LOG_TRACE_DISP_REFR 0
#define LV_LOG_TRACE_EVENT     0
#define LV_LOG_TRACE_OBJ_CREATE 0
#define LV_LOG_TRACE_LAYOUT    0
#define LV_LOG_TRACE_ANIM      0

/*==========================
   Assert — attivi per debug
===========================*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_ASSERT_HANDLER_INCLUDE   <stdint.h>
#define LV_ASSERT_HANDLER           while(1);

/*==========================
   Performance monitor
   Attivare per debug frame rate (disabilitare in produzione)
===========================*/
#define LV_USE_PERF_MONITOR  0
#define LV_USE_MEM_MONITOR   0
#define LV_USE_REFR_DEBUG    0

/*==========================
   Misc
===========================*/
#define LV_USE_USER_DATA      1
#define LV_ENABLE_GC          0
#define LV_BIG_ENDIAN_SYSTEM  0

// FIX [3]: VUOTO — non forzare il codice LVGL in IRAM
// Con IRAM_ATTR → overflow IRAM → crash EXCVADDR 0x83xxxxxx
// Senza → LVGL resta in Flash (DROM, XIP cached) — stabile
#define LV_ATTRIBUTE_FAST_MEM

#define LV_ATTRIBUTE_DMA
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning
#define LV_USE_LARGE_COORD    0

#endif /* LV_CONF_H */
#endif
