# Forno Pizza S3 — ESP32-S3-WROOM-1

Porting del controller forno pizza da ESP32 (ILI9341 SPI 320×240)
a **ESP32-S3-WROOM-1** con display RGB parallelo 480×272.

---

## Hardware

| Componente | Modello |
|---|---|
| MCU | ESP32-S3-WROOM-1-N4R2 (4MB flash, 2MB PSRAM) |
| Display | TST043WQHS-67 — 4.3" 480×272 RGB565 (SC7283) |
| Touch | FT6x36 — I2C capacitivo |
| Sensori T | MAX6675 × 2 — SPI |
| Relè | 3× meccanici (Base, Cielo, Luce) |

---

## Pin vietati su ESP32-S3-WROOM-1-N4R2

| GPIO | Motivo | Stato |
|---|---|---|
| GPIO26–GPIO32 | SPI0/1 flash+PSRAM interni | ⛔ VIETATI |
| GPIO33–GPIO37 | SPI0/1 8-line con PSRAM QPI/OPI | ⛔ VIETATI (usati da noi come relè — vedi nota) |
| GPIO19, GPIO20 | USB D-/D+ | ⚠️ Evitare |
| GPIO0 | Strapping BOOT (pulled-up) | ⚠️ Cautela |
| GPIO3 | Strapping JTAG (floating) | ⚠️ Cautela |
| GPIO45, GPIO46 | Strapping VDD_SPI / ROM print | ⚠️ Cautela |
| GPIO43, GPIO44 | UART0 console | ⚠️ Evitare |
| GPIO39–GPIO42 | JTAG (usabili senza debugger HW) | ✅ Usati per touch/display |

> **Nota GPIO33-35 (relè):** su N4R2 con PSRAM **QPI** (2MB) i GPIO33-37 sono liberi.
> Sarebbero vietati solo su N4R8 (8MB PSRAM **OPI**). Verifica il tuo modello esatto.

---

## Pinout

### Display RGB565 (16 bit)

| Segnale | GPIO | Note |
|---|---|---|
| R3–R7 | 4,5,6,7,8 | 5 bit rosso |
| G2–G7 | 9,10,11,12,13,14 | 6 bit verde |
| B3–B7 | 15,16,17,18,21 | 5 bit blu |
| DCLK | 47 | Pixel clock |
| HSYNC | 48 | Sync orizzontale |
| VSYNC | 38 | Sync verticale |
| DE | 39 | Data enable |
| DISP | 2 | Display ON — tieni HIGH |
| BL PWM | 1 | Backlight — LEDC ch0 |

> Pin flat non connessi: R0-R2 (pin 5-7), G0-G1 (pin 13-14), B0-B2 (pin 21-23) → collega a GND

### Touch FT6x36

| Segnale | GPIO | Note |
|---|---|---|
| SDA | 40 | JTAG TDO — ok senza debugger HW |
| SCL | 41 | JTAG TDI — ok senza debugger HW |
| INT | 42 | JTAG TCK — ok senza debugger HW |
| RST | non connesso | |

### MAX6675 (SPI software)

| Segnale | GPIO | Note |
|---|---|---|
| SCK | 3 | Strapping JTAG floating — safe come output |
| MISO (SO) | 46 | Strapping ROM print pull-down — safe come input |
| CS Base | 45 | Strapping VDD_SPI — libero su N4R2 |
| CS Cielo | 0 | Strapping BOOT pulled-up = CS inattivo al boot ✅ |

> Se questi pin causano problemi al boot, passa all'**Opzione B** in hardware.h
> (usa GPIO33-36 per SPI, liberando i pin strapping)

### Relè

| Relè | GPIO | Logica | Note |
|---|---|---|---|
| Base | 33 | HIGH = ON | OK su N4R2 (QPI PSRAM) |
| Cielo | 34 | HIGH = ON | OK su N4R2 (QPI PSRAM) |
| Luce | 35 | HIGH = ON | OK su N4R2 (QPI PSRAM) |

### GPIO liberi

| GPIO | Stato |
|---|---|
| 22, 23, 24, 25 | Non esistono su ESP32-S3 |
| 43, 44 | UART0 console — non usare |
| 36, 37 | ⚠️ Riservati PSRAM su N4R8, liberi su N4R2 |

---

## Librerie necessarie (Arduino Library Manager)

```
LovyanGFX        >= 1.1.12
lvgl             >= 8.3.0
Adafruit FT6206  (touch FT6x36)
MAX6675 library  (Adafruit)
PID              (Brett Beauregard)
Preferences      (inclusa in ESP32 Arduino core)
```

### Posizione lv_conf.h

Copia `lv_conf.h` in:
```
Documents/Arduino/libraries/lv_conf.h
```
(stesso livello della cartella `lvgl`, NON dentro di essa)

---

## File del progetto

| File | Descrizione |
|---|---|
| `FornoPizza_S3.ino` | Main — setup, task FreeRTOS |
| `hardware.h` | Pin define, macro relay, mutex |
| `display_driver.h` | LovyanGFX config + LVGL flush callback |
| `touch_driver.h` | FT6x36 I2C + LVGL touch callback |
| `lv_conf.h` | Configurazione LVGL 8.3.x |
| `pid_ctrl.h` | Controller PID + relay duty cycle |
| `nvs_storage.h/.cpp` | Persistenza impostazioni (NVS flash) |
| `ui.h/.cpp` | Schermate LVGL |
| `ui_events.cpp` | Gestione eventi touch UI |
| `autotune.h/.cpp` | Auto-tune PID (metodo relay) |

> **Nota:** `ui.h`, `ui.cpp`, `ui_events.cpp`, `pid_ctrl.h`, `nvs_storage.*`, `autotune.*`
> sono **identici** alla versione ESP32 originale — non richiedono modifiche.

---

## Differenze rispetto alla versione ESP32

| Aspetto | ESP32 originale | ESP32-S3 (questa versione) |
|---|---|---|
| Display | ILI9341 SPI 320×240 | RGB parallelo 480×272 |
| Display lib | TFT_eSPI | LovyanGFX |
| Touch | XPT2046 (resistivo) | FT6x36 (capacitivo) |
| Touch lib | XPT2046_Touchscreen | Adafruit_FT6206 |
| Backlight | GPIO21 | GPIO1 (LEDC) |
| Ventola | Relè GPIO14 | Non presente (GPIO31 libero) |
| WiFi/MQTT | Presente | **Disabilitato** (da aggiungere) |
| lv_conf.h | 320×240, SWAP=1 | 480×272, SWAP=0 |

---

## Note importanti

### LV_COLOR_16_SWAP = 0
Con il display RGB parallelo su ESP32-S3 il byte swap **non** è necessario
(diversamente dall'ILI9341 SPI che richiedeva SWAP=1).
Se i colori appaiono invertiti, prova a cambiare a `1`.

### PSRAM
Con 2MB PSRAM non è possibile fare double framebuffer (480×272×2×2 = 520KB).
Il progetto usa un singolo draw buffer LVGL da ~26KB in SRAM interna.
Per doppio framebuffer servirebbero almeno 4MB PSRAM (es. N4R8).

### Touch calibrazione
Se il touch risulta sfasato rispetto ai widget, modifica la funzione
`lv_touch_read_cb` in `touch_driver.h` — aggiusta i valori di `map()`.

### Timing display SC7283
I valori HSYNC/VSYNC back/front porch in `hardware.h` sono tipici per il
controller SC7283. Se lo schermo lampeggia o ha artefatti, consulta il
datasheet del display e aggiusta i timing.

---

## Aggiunta WiFi/MQTT (fase futura)

Quando vorrai aggiungere WiFi:
1. Ri-aggiungere `wifi_mqtt.h/.cpp` dalla versione ESP32
2. Aggiungere `Task_WiFi` nel setup
3. Aggiornare `Task_LVGL` con `case Screen::WIFI_SCAN` e `Screen::OTA`
4. I GPIO 43/44 (UART0) sono già riservati per la console seriale
