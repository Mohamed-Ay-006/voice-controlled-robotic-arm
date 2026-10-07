#ifndef CONFIG_H
#define CONFIG_H

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#if F_CPU != 8000000UL
#error "This firmware is configured for the verified 8 MHz crystal"
#endif

#define UART_BAUD                  9600UL
#define TWI_HZ                   100000UL
#define PCA9685_ADDRESS              0x40U
#define PCA9685_PRESCALE             121U

/* Six logical channel slots are retained so the gripper stays on CH5.
 * CH4 (wrist rotation) is disabled; the physical build has five servos. */
#define JOINT_COUNT                    6U
#define COMMAND_STEP_DEG               5
#define MOTION_STEP_DEG                1
#define MOTION_PERIOD_MS              20UL
#define LINK_TIMEOUT_MS             1000UL
#define CALIBRATION_TIMEOUT_MS       3000UL

/*
 * Leave this at zero until every servo has been tested unloaded, its horn has
 * been installed at HOME, and min/max/invert values have been measured.
 */
#define SYSTEM_COMMISSIONED             0

/* No independent servo supply exists. Keep every servo test command locked. */
#define CALIBRATION_MODE                0

/* PD7 -> PCA9685 OE. An external 10k pull-up to PCA VCC is mandatory. */
#define SERVO_OE_DDR                 DDRD
#define SERVO_OE_PORT               PORTD
#define SERVO_OE_PIN                  PD7

/* One active-low shoulder limit switch on PD2/INT0 with internal pull-up. */
#define LIMIT_DDR                    DDRD
#define LIMIT_PORT                  PORTD
#define LIMIT_PIN_REG                PIND
#define LIMIT_PIN                     PD2

/*
 * Physical homing is deliberately locked until the installation is observed.
 * Set to -1 if decreasing logical shoulder angle approaches the switch, or +1
 * if increasing angle approaches it. Zero rejects HOME safely.
 */
#define SHOULDER_HOME_DIRECTION         0
#define SHOULDER_SWITCH_ANGLE           25U
#define HOME_TIMEOUT_MS              8000UL

#endif
