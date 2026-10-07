# Final wiring

## ATmega32A to PCA9685

| ATmega32A | PCA9685 |
|---|---|
| PC0 / SCL (DIP 22) | SCL |
| PC1 / SDA (DIP 23) | SDA |
| PD7 (DIP 21) | OE |
| 5 V logic | VCC |
| GND | GND |

Add a 10 kΩ pull-up from `OE` to PCA9685 `VCC`. Leave address pads A0-A5 open. `V+` is unused because servo power bypasses the PCA board.

## ESP32-S3 to logic-level converter to ATmega32A

```text
ESP32 3V3      -> LV
ATmega 5V      -> HV
Common GND     -> both converter GND pins
ESP GPIO17 TX  -> LV1 / HV1 -> ATmega PD0 RXD (DIP 14)
ATmega PD1 TXD -> HV2 / LV2 -> ESP GPIO18 RX
```

## INMP441 to ESP32-S3

```text
VDD -> 3V3
GND -> GND
SCK -> GPIO5
WS  -> GPIO6
SD  -> GPIO7
L/R -> GND
```

Never power the INMP441 from 5 V.

## OLED to ESP32-S3

```text
VCC -> 3V3
GND -> GND
SDA -> GPIO8
SCL -> GPIO9
```

## Mechanical limit switch to ATmega32A

For the installed red/black/green module cable:

```text
Red   -> ATmega 5 V
Black -> ATmega GND
Green -> ATmega PD2 / INT0 (DIP 16)
```

## Servo signals

Only each servo's orange/yellow signal wire connects to the PCA9685:

```text
CH0 -> Base MG995 signal
CH1 -> Shoulder MG996 signal
CH2 -> Elbow MG996 signal
CH3 -> Wrist-pitch SG90 signal
CH4 -> not connected
CH5 -> Gripper SG90 signal
```

## Split servo power used by the prototype

The positive outputs must remain separate.

```text
5 V / 3 A supply positive -> Shoulder MG996 red
                          -> Elbow MG996 red

5 V / 2 A supply positive -> Base MG995 red
                          -> Wrist-pitch SG90 red
                          -> Gripper SG90 red
```

All negative/ground lines join at a common distribution point:

```text
5 V / 3 A supply GND
5 V / 2 A supply GND
all servo brown/black wires
PCA9685 GND
ATmega32A GND
ESP32-S3 GND
logic-converter GND
```

Do not join the two positive charger outputs. Do not connect the battery holder, the burned 7805, or either charger positive output to ESP32 `3V3`.

