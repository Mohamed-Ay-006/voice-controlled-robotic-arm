/* ESP32-S3 (N8R2) voice front-end for the ATmega32 robot arm  -  Arduino version
 *
 *   INMP441 mic (I2S, 16 kHz) -> MFCC -> DS-CNN -> keyword -> "OPEN\n" etc. on Serial1 @ 9600
 *
 * Arduino IDE settings (Tools menu):
 *   Board: "ESP32S3 Dev Module"       (esp32 by Espressif Systems, version 3.x)
 *   PSRAM: "QSPI PSRAM"               <-- REQUIRED for N8R2 (needs ~320 KB of PSRAM)
 *   Flash Size: 8MB                   Partition Scheme: "8M with spiffs" (or any with >= 1.5 MB app)
 *   CPU Frequency: 240MHz             USB CDC On Boot: Enabled if you use the native "USB" port,
 *                                                      Disabled if you use the "UART" port
 *
 * Pins, thresholds and the pin map live in kws_config.h.
 * Silence / unknown words / low confidence send NOTHING. ARM is only sent by the BOOT button. */
#include <Arduino.h>
#include "driver/i2s_std.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "kws_config.h"
#include "mfcc.h"
#include "kws_net.h"

/* The supplied weights contain only: silence, unknown, down, go, on and stop.
 * Never invent meanings for labels that were not trained for a required action.
 * "go" is intentionally ignored; the model has no "up" or "off" class. */
static const struct { const char *word, *cmd; } MAP[] = {
  {"down", "DOWN"}, {"on", "OPEN"}, {"stop", "STOP"},
};

static i2s_chan_handle_t s_rx;
#define RING 32768                        /* power of two, int16 samples */
static int16_t s_ring[RING];
static volatile uint32_t s_widx;          /* total samples written */
static float s_pcm[KWS_CLIP_SAMPLES];
static float s_mfcc[KWS_MFCC_COEFFS * KWS_MFCC_FRAMES];
static SemaphoreHandle_t s_uart_lock;

static void send_uart_line(const char *cmd, bool log_line) {
  char b[24]; int n = snprintf(b, sizeof b, "%s\n", cmd);
  if (s_uart_lock && xSemaphoreTake(s_uart_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial1.write((const uint8_t *)b, n);     /* one complete line for the AVR parser */
    Serial1.flush();
    xSemaphoreGive(s_uart_lock);
    if (log_line) Serial.printf(">>> UART: %s\n", cmd);
  }
}

static void send_cmd(const char *cmd) {
  send_uart_line(cmd, true);
}

static const char *cmd_for(const char *word) {
  for (size_t i = 0; i < sizeof MAP / sizeof MAP[0]; i++) if (!strcmp(word, MAP[i].word)) return MAP[i].cmd;
  return NULL;
}

static void init_i2s(void) {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  cc.dma_desc_num = 8; cc.dma_frame_num = 256;
  ESP_ERROR_CHECK(i2s_new_channel(&cc, NULL, &s_rx));
  i2s_std_config_t sc = {};
  sc.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(KWS_SAMPLE_RATE);
  sc.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;      /* INMP441 with L/R tied to GND */
  sc.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  sc.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
  sc.gpio_cfg.ws   = (gpio_num_t)PIN_I2S_WS;
  sc.gpio_cfg.dout = I2S_GPIO_UNUSED;
  sc.gpio_cfg.din  = (gpio_num_t)PIN_I2S_DIN;
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &sc));
  ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
}

/* ---------------- tasks ---------------- */
static void capture_task(void *arg) {
  static int32_t buf[512];
  uint32_t w = 0;
  for (;;) {
    size_t got = 0;
    if (i2s_channel_read(s_rx, buf, sizeof buf, &got, 1000) != ESP_OK) continue;
    size_t n = got / sizeof(int32_t);
    for (size_t i = 0; i < n; i++) s_ring[(w + i) & (RING - 1)] = (int16_t)(buf[i] >> 16);  /* INMP441: 24-bit MSB-aligned */
    w += n; s_widx = w;
  }
}

static void avr_rx_task(void *arg) {           /* just prints what the ATmega32 answers */
  char line[96]; int n = 0;
  for (;;) {
    while (Serial1.available()) {
      char c = (char)Serial1.read();
      if (c == '\n' || c == '\r') {
        if (n) {
          line[n] = 0;
          if (strcmp(line, "OK PONG") != 0) Serial.printf("AVR: %s\n", line);
          n = 0;
        }
      }
      else if (n < (int)sizeof line - 1) line[n++] = c;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static void infer_task(void *arg) {
  float *a = (float *)heap_caps_malloc(KWS_NET_WORK_FLOATS * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  float *b = (float *)heap_caps_malloc(KWS_NET_WORK_FLOATS * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!a || !b) {
    Serial.println("WARNING: no PSRAM -> Tools > PSRAM must be 'QSPI PSRAM'. Trying internal RAM...");
    a = (float *)malloc(KWS_NET_WORK_FLOATS * 4); b = (float *)malloc(KWS_NET_WORK_FLOATS * 4);
  }
  if (!a || !b) { Serial.println("ERROR: out of memory. Enable PSRAM (QSPI) in the Tools menu and re-upload."); vTaskDelete(NULL); }
  kws_net_set_work(a, b);
  const int nc = kws_num_classes();

  uint32_t last_run = 0; int streak = 0, last_cls = -1; int64_t lockout_until = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(20));
    uint32_t w = s_widx;
    if (w < KWS_CLIP_SAMPLES || (uint32_t)(w - last_run) < HOP_SAMPLES) continue;
    last_run = w;

    /* snapshot the newest 1.000 s, remove DC */
    double mean = 0;
    for (int i = 0; i < KWS_CLIP_SAMPLES; i++) { s_pcm[i] = s_ring[(w - KWS_CLIP_SAMPLES + i) & (RING - 1)] / 32768.0f; mean += s_pcm[i]; }
    mean /= KWS_CLIP_SAMPLES;
    float peak = 0, loud = 0;
    for (int i = 0; i < KWS_CLIP_SAMPLES; i++) { s_pcm[i] -= (float)mean; float m = fabsf(s_pcm[i]); if (m > peak) peak = m; }
    for (int blk = 0; blk < KWS_CLIP_SAMPLES; blk += 320) {          /* loudest 20 ms block */
      float e = 0; for (int i = 0; i < 320; i++) e += s_pcm[blk + i] * s_pcm[blk + i];
      e = sqrtf(e / 320); if (e > loud) loud = e;
    }
    if (loud < GATE_RMS) { streak = 0; last_cls = -1; continue; }   /* quiet: nothing to classify */
#if PEAK_NORMALIZE
    { float g = PEAK_TARGET / peak; for (int i = 0; i < KWS_CLIP_SAMPLES; i++) s_pcm[i] *= g; }
#endif
    int64_t t0 = esp_timer_get_time();
    mfcc_compute(s_pcm, s_mfcc);
    float lg[KWS_MAX_CLASSES], p[KWS_MAX_CLASSES];
    kws_net_run(s_mfcc, lg);
    int64_t t1 = esp_timer_get_time();
    float mx = lg[0]; for (int k = 1; k < nc; k++) if (lg[k] > mx) mx = lg[k];
    float sum = 0; for (int k = 0; k < nc; k++) { p[k] = expf(lg[k] - mx); sum += p[k]; }
    int best = 0; for (int k = 0; k < nc; k++) { p[k] /= sum; if (p[k] > p[best]) best = k; }

    const char *word = kws_label(best), *cmd = cmd_for(word);
    bool is_stop = !strcmp(word, "stop");
    float th = is_stop ? CONF_STOP : CONF_DEFAULT; int need = is_stop ? STREAK_STOP : STREAK_DEFAULT;
#if LOG_EVERY_WINDOW
    Serial.printf("loud=%.3f peak=%.3f  %s %.2f  (%lld ms)\n", loud, peak, word, p[best], (long long)((t1 - t0) / 1000));
#endif
    if (!cmd || p[best] < th) { streak = 0; last_cls = -1; continue; }
    streak = (best == last_cls) ? streak + 1 : 1; last_cls = best;
    if (streak >= need && t1 >= lockout_until) {
      send_cmd(cmd); lockout_until = t1 + (int64_t)LOCKOUT_MS * 1000; streak = 0; last_cls = -1;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);                                   /* let the USB serial monitor attach */
  Serial.println("\nKWS -> ATmega32 (Arduino)  classes:");
  for (int i = 0; i < kws_num_classes(); i++) {
    const char *c = cmd_for(kws_label(i));
    Serial.printf("  %d %-10s -> %s\n", i, kws_label(i), c ? c : "(no command)");
  }
  Serial.printf("PSRAM: %s (%u bytes)\n", psramFound() ? "found" : "NOT FOUND - set Tools > PSRAM = QSPI PSRAM", (unsigned)ESP.getPsramSize());
  Serial1.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
  s_uart_lock = xSemaphoreCreateMutex();
  if (!s_uart_lock) {
    Serial.println("ERROR: cannot create UART mutex");
    while (true) delay(1000);
  }
  pinMode(PIN_ARM_BTN, INPUT_PULLUP);
  init_i2s();
  xTaskCreatePinnedToCore(capture_task, "capture", 4096, NULL, 6, NULL, 0);
  xTaskCreatePinnedToCore(avr_rx_task, "avr_rx", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(infer_task, "kws", 8192, NULL, 4, NULL, 1);
}

void loop() {                                    /* BOOT button -> ARM */
  static int prev = 1;
  static uint32_t last_heartbeat = 0;
  const uint32_t now_ms = millis();

  if ((uint32_t)(now_ms - last_heartbeat) >= HEARTBEAT_MS) {
    last_heartbeat = now_ms;
    send_uart_line("PING", false);
  }

  int now = digitalRead(PIN_ARM_BTN);
  if (prev == 1 && now == 0) { delay(30); if (digitalRead(PIN_ARM_BTN) == 0) send_cmd("ARM"); }
  prev = now; delay(20);
}
