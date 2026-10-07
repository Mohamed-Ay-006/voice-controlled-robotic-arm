#pragma once
/* ================= PINS (ESP32-S3 N8R2) =================
 * GPIO26-32 = flash/PSRAM (never use).  GPIO0/3/45/46 = strapping pins.
 * Final INMP441 wiring: VDD->3V3, GND->GND, L/R->GND,
 *                       SCK->GPIO5, WS->GPIO6, SD->GPIO7. */
#define PIN_I2S_BCLK   5
#define PIN_I2S_WS     6
#define PIN_I2S_DIN    7

/* Reserved for the final 1.3-inch I2C OLED connection.  The display controller
 * (SH1106 or SSD1306) must be confirmed before enabling a display driver. */
#define PIN_OLED_SDA   8
#define PIN_OLED_SCL   9

/* UART through the 4-channel bidirectional logic-level converter:
 * ESP TX GPIO17 -> LV1/HV1 -> AVR PD0(RXD)
 * AVR PD1(TXD)  -> HV2/LV2 -> ESP RX GPIO18
 * Converter LV=3V3, HV=ATmega 5V, and all grounds common. */
#define PIN_UART_TX    17
#define PIN_UART_RX    18
#define UART_BAUD      9600
#define HEARTBEAT_MS   500
/* Push-button on the BOOT key (GPIO0, active low): sends "ARM". Voice can never arm the robot. */
#define PIN_ARM_BTN    0

/* ================= AUDIO ================= */
#define HOP_SAMPLES     4000     /* run inference every 250 ms (raise to 8000 if inference takes >250 ms) */
#define GATE_RMS        0.004f   /* skip inference unless some 20 ms block is louder than this (full scale = 1.0) */
#define PEAK_NORMALIZE  1        /* scale each 1 s window so its peak = PEAK_TARGET (removes mic-gain mismatch) */
#define PEAK_TARGET     0.5f
#define LOG_EVERY_WINDOW 1       /* print class probabilities for every window that passes the gate */

/* ================= DECISION ================= */
#define CONF_DEFAULT    0.85f    /* same threshold as the notebook */
#define CONF_STOP       0.70f    /* STOP is allowed to fire more easily (safer to stop than to move) */
#define STREAK_DEFAULT  2        /* consecutive windows that must agree */
#define STREAK_STOP     1
#define LOCKOUT_MS      1200     /* ignore new commands for this long after one was sent */
