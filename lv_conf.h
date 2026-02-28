/**
 * lv_conf.h — LVGL 8.3.x per ESP32-S3-WROOM-1-N4R8
 * POSIZIONE: Documents/Arduino/libraries/lv_conf.h
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
#define LV_COLOR_16_SWAP  0

/*==========================
   Memoria LVGL heap
===========================*/
#define LV_MEM_CUSTOM     0
#define LV_MEM_SIZE       (64U * 1024U)
#define LV_MEM_ADR        0
//#define LV_MEM_POOL_INCLUDE ""
#define LV_MEM_POOL_ALLOC  lv_mem_alloc
#define LV_MEM_POOL_FREE   lv_mem_free

/*==========================
   Tick & refresh
===========================*/
#define LV_DISP_DEF_REFR_PERIOD  20
#define LV_INDEV_DEF_READ_PERIOD 30

/*==========================
   Font Montserrat
   Analisi utilizzo nel codice:
     10  → label ±5°C (ui.cpp)
     12  → PID%, status, splash
     14  → DEFAULT — usato ovunque
     16  → header panel, °C, WiFi
     18  → header WiFi/OTA, ricette
     20  → NON USATO → 0
     22  → setpoint arco CTRL (TempSetBase/Cielo) → 1 ← era 0, FIX
     24  → NON USATO → 0
     26  → NON USATO → già 0
     28  → bottoni ± (make_pm) → 1 ← portato a 1, FIX
     32  → Kp/Ki/Kd label PID → 1 ← era 0, FIX
     36  → splash titolo
     48  → temperatura MAIN
===========================*/
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  1
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_18  1
#define LV_FONT_MONTSERRAT_20  0   /* non usato */
#define LV_FONT_MONTSERRAT_22  1   /* FIX: setpoint arco CTRL */
#define LV_FONT_MONTSERRAT_24  0   /* non usato */
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  1   /* FIX: bottoni ± make_pm in ui.cpp */
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  1   /* FIX: Kp/Ki/Kd schermate PID */
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  1   /* splash titolo */
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  1   /* temperatura MAIN */
#define LV_FONT_DEFAULT        &lv_font_montserrat_14

#define LV_FONT_UNSCII_8       0
#define LV_FONT_UNSCII_16      0
#define LV_FONT_SIMSUN_16_CJK  0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0

/*==========================
   Rendering
===========================*/
#define LV_DRAW_COMPLEX       1
#define LV_SHADOW_CACHE_SIZE  0
#define LV_CIRCLE_CACHE_SIZE  4
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_GRADIENT_MAX_STOPS 2
#define LV_GRAD_CACHE_DEF_SIZE 0

/*==========================
   Animazioni
===========================*/
#define LV_USE_ANIMATION      1

/*==========================
   Widget
===========================*/
#define LV_USE_ARC            1
#define LV_USE_BAR            1
#define LV_USE_BTN            1
#define LV_USE_BTNMATRIX      1
#define LV_USE_CANVAS         0
#define LV_USE_CHECKBOX       0
#define LV_USE_DROPDOWN       0
#define LV_USE_IMG            1
#define LV_USE_LABEL          1
#define LV_USE_LINE           1
#define LV_USE_ROLLER         0
#define LV_USE_SLIDER         0
#define LV_USE_SWITCH         0
#define LV_USE_TEXTAREA       1
#define LV_USE_TABLE          0
#define LV_USE_CHART          1
#define LV_USE_LED            1
#define LV_USE_METER          0
#define LV_USE_MSGBOX         0
#define LV_USE_SPINBOX        0
#define LV_USE_SPINNER        0
#define LV_USE_TABVIEW        0
#define LV_USE_TILEVIEW       0
#define LV_USE_WIN            0
#define LV_USE_SPAN           0
#define LV_USE_CALENDAR       0
#define LV_USE_COLORWHEEL     0
#define LV_USE_IMGBTN         0
#define LV_USE_KEYBOARD       1
#define LV_USE_LIST           1
#define LV_USE_MENU           1

/*==========================
   Tema scuro
===========================*/
#define LV_USE_THEME_DEFAULT  1
#define LV_THEME_DEFAULT_DARK 1
#define LV_USE_THEME_BASIC    0
#define LV_USE_THEME_MONO     0

/*==========================
   Testo
===========================*/
#define LV_TXT_ENC            LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS    " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD      "#"
#define LV_USE_BIDI           0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==========================
   Log
===========================*/
#define LV_USE_LOG            1
#define LV_LOG_LEVEL          LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF         1
#define LV_LOG_TRACE_MEM      0
#define LV_LOG_TRACE_TIMER    0
#define LV_LOG_TRACE_INDEV    0
#define LV_LOG_TRACE_DISP_REFR 0
#define LV_LOG_TRACE_EVENT    0
#define LV_LOG_TRACE_OBJ_CREATE 0
#define LV_LOG_TRACE_LAYOUT   0
#define LV_LOG_TRACE_ANIM     0

/*==========================
   Assert
===========================*/
#define LV_USE_ASSERT_NULL        1
#define LV_USE_ASSERT_MALLOC      1
#define LV_USE_ASSERT_STYLE       0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ         0
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER         while(1);

/*==========================
   Performance monitor
===========================*/
#define LV_USE_PERF_MONITOR   0
#define LV_USE_MEM_MONITOR    0
#define LV_USE_REFR_DEBUG     0

/*==========================
   Misc
===========================*/
#define LV_USE_USER_DATA      1
#define LV_ENABLE_GC          0
#define LV_BIG_ENDIAN_SYSTEM  0
#define LV_ATTRIBUTE_FAST_MEM IRAM_ATTR
#define LV_ATTRIBUTE_DMA
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning
#define LV_USE_LARGE_COORD    0

#endif /* LV_CONF_H */
#endif
