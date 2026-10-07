# ATmega32A motion controller

Target: ATmega32A, 8 MHz external crystal.

## Pin map

```text
PD0/RXD -> level-converter HV1
PD1/TXD -> level-converter HV2
PD2     -> mechanical limit switch
PD7     -> PCA9685 OE
PC0     -> PCA9685 SCL
PC1     -> PCA9685 SDA
```

## Channels

```text
CH0 base MG995
CH1 shoulder MG996
CH2 elbow MG996
CH3 wrist-pitch SG90
CH4 disabled
CH5 gripper SG90
```

## Build

```text
make
```

The repository also includes a verified prebuilt HEX in `build/robot_arm_atmega32a.hex`.

The firmware is safe by default. `SYSTEM_COMMISSIONED` and `CALIBRATION_MODE` remain disabled until the physical arm's limits have been measured.

