# Porting JC4827W543C — Forno Pizza S3

## Hardware target
- **Scheda display**: JC4827W543C (480×272)
- **Controller LCD**: ST7701A (compatibile ST7701S in LovyanGFX)
- **Touch**: CST820 capacitivo I2C
- **MCU**: ESP32-S3-WROOM-1-N4R2

## File modificati
| File | Modifica |
|------|----------|
| `hardware.h` | Pinout completo per JC4827W543C + 4° relè ventola GPIO10 |
| `display_driver.h` | Da RGB parallelo (SC7283) → SPI 4-wire ST7701S |
| `touch_driver.h` | Da FT6x36 (Adafruit_FT6206) → CST820 via Wire raw |
| `FornoPizza_S3.ino` | Rimossa istanza FT6x36, aggiunto RELAY_FAN, logica ventola automatica |

## Pinout JC4827W543C
### LCD ST7701A — SPI 4-wire
| Segnale | GPIO |
|---------|------|
| MOSI    | 6    |
| CLK     | 7    |
| CS      | 5    |
| DC      | 4    |
| RST     | 48   |
| BL (PWM)| 45   |

### Touch CST820 — I2C
| Segnale | GPIO |
|---------|------|
| SDA     | 8    |
| SCL     | 9    |
| INT     | 3    |
| RST     | 38   |
| Addr    | 0x15 |

### MAX6675 — SPI software
| Segnale  | GPIO |
|----------|------|
| SCK      | 11   |
| MISO     | 12   |
| CS_BASE  | 13   |
| CS_CIELO | 14   |

### Relè (HIGH=ON, logica invertibile via _INV define)
| Relè   | GPIO |
|--------|------|
| BASE   | 15   |
| CIELO  | 16   |
| LUCE   | 17   |
| FAN    | 10   |

## Librerie necessarie (Library Manager)
- **LovyanGFX** >= 1.1.12
- **lvgl** >= 8.3.0 (con lv_conf.h nella cartella sketch)
- **max6675** (Adafruit MAX6675)
- **PID_v1** (Brett Beauregard)
- **Preferences** (inclusa in ESP32 Arduino core)

> ⚠️ Non serve più `Adafruit_FT6206` — il CST820 è gestito via Wire diretto in `touch_driver.h`

## Note touch
Se le coordinate risultano specchiate o ruotate rispetto allo schermo,
decommentare le righe in `touch_driver.h`:
```cpp
// x = LCD_H_RES - 1 - x;
// y = LCD_V_RES - 1 - y;
```

## Logica ventola
La ventola (RELAY_FAN) è gestita automaticamente nel Task_PID:
- **ON** se almeno una resistenza è attiva (BASE o CIELO relay=true)
- **ON** se temperatura BASE o CIELO > `FAN_OFF_TEMP` (80°C)
- **OFF** altrimenti

Per invertire la logica del relè ventola: impostare `RELAY_FAN_INV true` in `hardware.h`.
