/**
 * touch_driver.h — CST820 I2C touch driver per LVGL
 *
 * Controller: CST820 (Hynitron)
 * Scheda:     JC4827W543C
 * I2C:        SDA=8  SCL=9  INT=3  RST=38
 * Address:    0x15
 *
 * Il CST820 non ha una libreria Arduino standard popolare.
 * Questo driver legge i registri via Wire direttamente.
 *
 * REGISTRI CST820:
 *   0x00 → GestureID   (0 = nessun gesto)
 *   0x01 → FingerNum   (numero dita)
 *   0x02 → XposH[7:4] | Event[1:0]
 *   0x03 → XposL[7:0]
 *   0x04 → YposH[3:0]
 *   0x05 → YposL[7:0]
 *
 * NOTA COORDINATE:
 *   Il CST820 del JC4827W543C restituisce X su [0..479] e Y su [0..271].
 *   Se il touch è specchiato rispetto allo schermo, invertire:
 *     x = LCD_H_RES - 1 - x
 *     y = LCD_V_RES - 1 - y
 */

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include "hardware.h"

// ================================================================
//  LETTURA REGISTRO CST820
// ================================================================
static bool cst820_read_reg(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, len);
  for (uint8_t i = 0; i < len && Wire.available(); i++) {
    buf[i] = Wire.read();
  }
  return true;
}

// ================================================================
//  CALLBACK LVGL — lettura touch
// ================================================================
static void lv_touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  uint8_t buf[6] = {0};

  if (!cst820_read_reg(0x00, buf, 6)) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  uint8_t finger_num = buf[1] & 0x0F;

  if (finger_num == 0) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  // Ricostruzione coordinate 12-bit
  uint16_t x = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];
  uint16_t y = ((uint16_t)(buf[4] & 0x0F) << 8) | buf[5];

  // Clamp alle dimensioni display
  if (x >= LCD_H_RES) x = LCD_H_RES - 1;
  if (y >= LCD_V_RES) y = LCD_V_RES - 1;

  // Se le coordinate risultano specchiate, decommenta le righe seguenti:
  // x = LCD_H_RES - 1 - x;
  // y = LCD_V_RES - 1 - y;

  data->point.x = (lv_coord_t)x;
  data->point.y = (lv_coord_t)y;
  data->state   = LV_INDEV_STATE_PR;
}

// ================================================================
//  touch_init() — chiama da setup() DOPO display_init()
// ================================================================
inline void touch_init() {
  // Reset hardware del CST820
  if (TOUCH_RST >= 0) {
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
  }

  // INT pin come input (il CST820 lo porta LOW quando c'è un tocco)
  if (TOUCH_INT >= 0) {
    pinMode(TOUCH_INT, INPUT);
  }

  // Avvia I2C su pin custom
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);   // 400 kHz I2C Fast Mode

  // Verifica presenza CST820
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.printf("[TOUCH] ERRORE: CST820 non trovato (addr=0x%02X, err=%d)\n",
      TOUCH_I2C_ADDR, err);
    // Non bloccare il boot — UI funziona comunque
  } else {
    Serial.printf("[TOUCH] CST820 OK — SDA=%d SCL=%d INT=%d RST=%d\n",
      TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST);
  }

  // Registra driver touch in LVGL
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touch_read_cb;
  lv_indev_drv_register(&indev_drv);
}
