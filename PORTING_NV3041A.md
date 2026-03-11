# Porting GUITION JC4827W543C — NV3041A + GT911

## Hardware target
| Componente | Dettaglio |
|---|---|
| Scheda | GUITION JC4827W543C |
| Controller LCD | NV3041A — QSPI (Quad SPI) |
| Touch | GT911 — I2C capacitivo |
| MCU | ESP32-S3-WROOM-1-N4R8 |
| Flash | 4MB |
| PSRAM | 8MB OPI |

---

## Differenze critiche rispetto alle versioni precedenti

| Aspetto | Versione precedente | Questa versione |
|---|---|---|
| Display bus | RGB parallelo o SPI singolo | **QSPI (Quad SPI 4-wire)** |
| Panel driver | `Panel_RGB` / `Panel_ST7701S` | **`Panel_NV3041A`** |
| Bus driver | `Bus_RGB` / `Bus_SPI` | **`Bus_QSPI`** |
| Touch | CST820 / FT6x36 | **GT911** |
| Touch lib | `Touch_CST820` / Adafruit_FT6206 | **`Touch_GT911`** |
| Flash | ~~16MB~~ (ERRATO) | **4MB** |
| Partition | ~~default_16MB~~ (ERRATO) | **default** |
| GPIO 33-37 | Liberi su N4R2 | **⛔ RISERVATI su N4R8 (PSRAM OPI)** |

---

## File modificati

| File | Modifica principale |
|---|---|
| `display_driver.h` | `Panel_NV3041A` + `Bus_QSPI` + `Touch_GT911` |
| `hardware.h` | Pinout QSPI, GPIO relè spostati su pin liberi N4R8 |
| `.github/workflows/build.yml` | `FlashSize=4M`, `PartitionScheme=default` |

### File NON modificati (identici)
`ui.h`, `ui.cpp`, `ui_events.cpp`, `pid_ctrl.h`, `nvs_storage.*`,
`autotune.*`, `FornoPizza_S3.ino`, `lv_conf.h`, `debug_config.h`

---

## Pinout completo GUITION JC4827W543C

### Display NV3041A — QSPI
| Segnale | GPIO | Note |
|---|---|---|
| QSPI CLK | 47 | |
| QSPI D0 | 21 | |
| QSPI D1 | 48 | |
| QSPI D2 | 40 | |
| QSPI D3 | 39 | |
| CS | 45 | Strapping VDD_SPI — ok dopo boot |
| Backlight PWM | 1 | LEDC ch0 |

### Touch GT911 — I2C
| Segnale | GPIO | Note |
|---|---|---|
| SDA | 8 | |
| SCL | 4 | |
| INT | 3 | Strapping JTAG — ok come input dopo boot |
| RST | 38 | |
| Indirizzo I2C | 0x14 | Alternativo: 0x5D |

### MAX6675 — SPI software
| Segnale | GPIO | Note |
|---|---|---|
| SCK | 12 | |
| MISO | 13 | |
| CS Base | 14 | |
| CS Cielo | 15 | |

### Relè (HIGH=ON, modificabile con _INV)
| Relè | GPIO | Note |
|---|---|---|
| Base | 10 | Libero su N4R8 |
| Cielo | 11 | Libero su N4R8 |
| Luce | 17 | Libero su N4R8 |
| Fan | 18 | Libero su N4R8 |

---

## GPIO vietati su N4R8 (8MB PSRAM OPI)

⚠️ La PSRAM OPI a 8MB riserva i GPIO 33-37 per il bus interno a 8 linee.
Questa è la differenza principale rispetto all'N4R2 (2MB PSRAM QPI).

| GPIO | Motivo | Stato |
|---|---|---|
| 26–32 | SPI0/1 flash interno | ⛔ VIETATI |
| 33–37 | PSRAM OPI 8-line (N4R8!) | ⛔ VIETATI su N4R8 |
| 19–20 | USB D-/D+ | ⚠️ Evitare |
| 43–44 | UART0 console | ⚠️ Evitare |
| 0 | Strapping BOOT | ⚠️ Cautela |
| 3 | Strapping JTAG (usato da Touch INT) | ✅ Ok dopo boot |
| 45 | Strapping VDD_SPI (usato da LCD CS) | ✅ Ok dopo boot |

---

## Librerie necessarie

```
LovyanGFX    >= 1.1.12   ← Panel_NV3041A e Bus_QSPI inclusi
lvgl         >= 8.3.0
MAX6675 library (Adafruit)
PID          (Brett Beauregard)
Preferences  (inclusa in ESP32 Arduino core)
```

### Librerie rimosse rispetto alla versione precedente
```
❌ Adafruit_FT6206  — non serve (GT911 gestito da LovyanGFX)
❌ TFT_eSPI         — non serve
```

### Verifica Panel_NV3041A in LovyanGFX
```bash
find ~/Arduino/libraries/LovyanGFX -name "Panel_NV3041A.hpp"
# Deve trovare il file — se non trovato, aggiorna LovyanGFX
```

---

## Posizione lv_conf.h

Copia `lv_conf.h` nella cartella sketch:
```
FornoPizza_S3/lv_conf.h
```
oppure in:
```
Documents/Arduino/libraries/lv_conf.h
```

---

## Note touch GT911

Il GT911 è ruotato 180° rispetto al display NV3041A su questa scheda.
Nel `display_driver.h` le coordinate sono già specchiate automaticamente:
```cpp
x = LCD_H_RES - 1 - x;
y = LCD_V_RES - 1 - y;
```

Se il touch risulta comunque sfasato dopo la correzione:
1. Rimuovi le righe di specchio in `lv_touch_cb()`
2. Prova tutte le combinazioni di mirror_x / mirror_y

---

## Note NV3041A — colori

Se i colori appaiono invertiti (cyan invece di rosso, ecc.):
- `cfg.invert = true` è già impostato (corretto per NV3041A)
- Se comunque sbagliati, prova `cfg.invert = false`
- `LV_COLOR_16_SWAP` in `lv_conf.h` deve essere `0` con QSPI

---

## Procedura di debug consigliata

```
Step 1: TASK_PID=0  TASK_WDG=0  TASK_WIFI=0
        → verifica solo display + touch
        → deve apparire la splash screen

Step 2: Aggiungi TASK_PID=1
        → verifica lettura MAX6675 via seriale
        → [C1 ...] deve apparire ogni secondo

Step 3: TASK_WDG=1
        → il watchdog deve restare silenzioso

Step 4: TASK_WIFI=1 (se abilitato)
```
