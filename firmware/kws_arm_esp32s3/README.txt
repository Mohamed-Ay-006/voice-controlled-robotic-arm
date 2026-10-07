ESP32-S3 N8R2 - FINAL ESP32 VOICE FRONT-END

ROLE
----
INMP441 -> ESP32-S3 keyword model -> UART through the logic-level converter -> ATmega32A.
The ATmega32A, not the ESP32, controls the PCA9685 in the final circuit.

FINAL ESP32 CONNECTIONS
-----------------------
INMP441 VDD -> ESP32 3V3
INMP441 GND -> ESP32 GND
INMP441 L/R -> ESP32 GND
INMP441 SCK -> ESP32 GPIO5
INMP441 WS  -> ESP32 GPIO6
INMP441 SD  -> ESP32 GPIO7

OLED VCC -> ESP32 3V3
OLED GND -> ESP32 GND
OLED SDA -> ESP32 GPIO8
OLED SCL -> ESP32 GPIO9
NOTE: GPIO8/9 are reserved in this firmware. Display drawing is not enabled because
the supplied files do not identify whether the 1.3-inch display uses SH1106 or SSD1306.

ESP32 3V3      -> level converter LV
ESP32 GND      -> level converter GND
ESP32 GPIO17 TX -> converter LV1; converter HV1 -> ATmega PD0/RXD
ESP32 GPIO18 RX <- converter LV2; converter HV2 <- ATmega PD1/TXD
ATmega 5V      -> level converter HV

Do not connect ESP32 SDA/SCL/OE directly to the PCA9685 in the final circuit.

MODEL LIMITATION FOUND DURING REVIEW
------------------------------------
The supplied weights contain exactly these labels:
  _silence_, _unknown_, down, go, on, stop

The safe command map in this revision is:
  down -> DOWN
  on   -> OPEN
  stop -> STOP
  go   -> ignored

There is no trained "up" or "off" class in the supplied weights, so this package
cannot truthfully generate UP or CLOSE from voice until replacement weights are supplied.
The BOOT button sends ARM. Voice never sends ARM.

UART is 9600 8N1. A PING is sent every 500 ms to satisfy the ATmega link watchdog.

UPLOAD
------

1. Install Arduino IDE 2.x.  Tools > Board > Boards Manager > search "esp32" >
   install "esp32 by Espressif Systems" (version 3.x; compiled here against 3.3.12).
2. Unzip. Keep ALL files together in the folder named kws_arm_esp32s3 and open kws_arm_esp32s3.ino.
3. Tools menu:
     Board ............ ESP32S3 Dev Module
     PSRAM ............ QSPI PSRAM        <-- REQUIRED (N8R2 = 2 MB quad PSRAM). Without it the model runs out of memory.
     Flash Size ....... 8MB (64Mb)
     Partition Scheme . 8M with spiffs (3MB APP/1.5MB SPIFFS)
     CPU Frequency .... 240MHz (WiFi)
     USB CDC On Boot .. Disabled if you plug into the "UART" port, Enabled if you plug into the "USB" port
     Port ............. the COM port of the board
4. Click Upload (arrow). First build takes a few minutes. If it cannot connect:
   hold BOOT, tap RESET, release BOOT, click Upload again.
5. Tools > Serial Monitor, 115200 baud. Press RESET. You should see the class list and "PSRAM: found (2093...)".
6. Say "on", "down", "stop" ~30 cm from the mic: lines like  loud=0.031 peak=0.210  on 0.97  (180 ms)
   followed by  >>> UART: OPEN
   The BOOT button (after boot) sends ARM.

Pins / thresholds: kws_config.h.   Word -> command table: MAP[] near the top of the .ino.
