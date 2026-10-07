#include "config.h"
#include "limit.h"
#include "pca9685.h"
#include "servo.h"
#include "timebase.h"
#include "twi.h"
#include "uart.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LINE_CAPACITY 48U

typedef struct
{
    char text[LINE_CAPACITY];
    uint8_t used;
    bool discard;
    uint32_t started_at;
} line_parser_t;

static line_parser_t g_parser;
static bool g_armed;
static bool g_fault;
static bool g_link_lost;
static bool g_test_active;
static joint_t g_selected = JOINT_SHOULDER;
static uint32_t g_last_valid_command;
static uint32_t g_test_started_at;

void early_watchdog_disable(void) __attribute__((naked, section(".init3")));
void early_watchdog_disable(void)
{
    MCUCSR = 0U;
    wdt_disable();
}

static void outputs_disable(void)
{
    SERVO_OE_PORT |= (1U << SERVO_OE_PIN);
}

#if SYSTEM_COMMISSIONED || CALIBRATION_MODE
static void outputs_enable(void)
{
    SERVO_OE_PORT &= (uint8_t)~(1U << SERVO_OE_PIN);
}
#endif

static void enter_fault(const char *message)
{
    servo_stop();
    outputs_disable();
    g_fault = true;
    g_armed = false;
    g_test_active = false;
    (void)uart_send_line(message);
}

static int parser_feed(uint8_t value, uint32_t now_ms)
{
    bool allowed;

    if ((value == '\r') || (value == '\n'))
    {
        if (g_parser.discard)
        {
            g_parser.discard = false;
            g_parser.used = 0U;
            return -1;
        }
        if (g_parser.used == 0U)
        {
            return 0;
        }
        g_parser.text[g_parser.used] = '\0';
        g_parser.used = 0U;
        return 1;
    }

    allowed = ((value >= 'A') && (value <= 'Z')) ||
              ((value >= '0') && (value <= '9')) ||
              (value == '_') || (value == ' ');
    if (!allowed || (g_parser.used >= (LINE_CAPACITY - 1U)))
    {
        g_parser.used = 0U;
        g_parser.discard = true;
        return -1;
    }
    if (g_parser.used == 0U)
    {
        g_parser.started_at = now_ms;
    }
    g_parser.text[g_parser.used++] = (char)value;
    return 0;
}

static void parser_timeout(uint32_t now_ms)
{
    if ((g_parser.used != 0U) &&
        ((uint32_t)(now_ms - g_parser.started_at) >= 300UL))
    {
        g_parser.used = 0U;
        g_parser.discard = true;
        (void)uart_send_line("ERR LINE_TIMEOUT");
    }
}

static bool parse_uint(const char **cursor, uint16_t *result)
{
    uint16_t value = 0U;
    bool any = false;
    while ((**cursor >= '0') && (**cursor <= '9'))
    {
        any = true;
        value = (uint16_t)(value * 10U + (uint16_t)(**cursor - '0'));
        if (value > 999U)
        {
            return false;
        }
        ++(*cursor);
    }
    *result = value;
    return any;
}

static bool parse_test(const char *line, joint_t *joint, uint8_t *angle)
{
    const char *cursor = line;
    uint16_t parsed_joint;
    uint16_t parsed_angle;

    if (strncmp(cursor, "TEST ", 5U) != 0)
    {
        return false;
    }
    cursor += 5U;
    if (!parse_uint(&cursor, &parsed_joint) || (*cursor != ' '))
    {
        return false;
    }
    ++cursor;
    if (!parse_uint(&cursor, &parsed_angle) || (*cursor != '\0'))
    {
        return false;
    }
    if ((parsed_joint >= JOINT_COUNT) || (parsed_angle > 180U))
    {
        return false;
    }
    *joint = (joint_t)parsed_joint;
    *angle = (uint8_t)parsed_angle;
    return true;
}

static void send_status(void)
{
    char response[118];
    (void)snprintf(response, sizeof(response),
                   "OK STATUS ARM=%u FAULT=%u LINK=%u SEL=%u A=%u,%u,%u,%u,%u,%u",
                   g_armed ? 1U : 0U, g_fault ? 1U : 0U,
                   g_link_lost ? 0U : 1U, (unsigned)g_selected,
                   servo_angle(JOINT_BASE), servo_angle(JOINT_SHOULDER),
                   servo_angle(JOINT_ELBOW), servo_angle(JOINT_WRIST_PITCH),
                   servo_angle(JOINT_WRIST_ROTATE), servo_angle(JOINT_GRIPPER));
    (void)uart_send_line(response);
}

static bool select_command(const char *line)
{
    if (strcmp(line, "SELECT_BASE") == 0)
        g_selected = JOINT_BASE;
    else if (strcmp(line, "SELECT_SHOULDER") == 0)
        g_selected = JOINT_SHOULDER;
    else if (strcmp(line, "SELECT_ELBOW") == 0)
        g_selected = JOINT_ELBOW;
    else if (strcmp(line, "SELECT_WRIST_PITCH") == 0)
        g_selected = JOINT_WRIST_PITCH;
    else if (strcmp(line, "SELECT_GRIPPER") == 0)
        g_selected = JOINT_GRIPPER;
    else
        return false;
    return true;
}

static bool motion_allowed(void)
{
    if (g_fault)
    {
        (void)uart_send_line("ERR FAULT");
        return false;
    }
    if (!g_armed)
    {
        (void)uart_send_line("ERR DISARMED");
        return false;
    }
    if (g_link_lost)
    {
        (void)uart_send_line("ERR LINK");
        return false;
    }
    return true;
}

static void queue_delta(int8_t delta)
{
    if (!servo_move_delta(g_selected, delta))
    {
        (void)uart_send_line(servo_busy() ? "ERR BUSY" : "ERR RANGE");
    }
    else
    {
        (void)uart_send_line("OK QUEUED");
    }
}

static void handle_command(const char *line, uint32_t now_ms)
{
    joint_t test_joint;
    uint8_t test_angle;

    if (strcmp(line, "PING") == 0)
    {
        g_last_valid_command = now_ms;
        g_link_lost = false;
        (void)uart_send_line("OK PONG");
        return;
    }
    if (strcmp(line, "STATUS") == 0)
    {
        g_last_valid_command = now_ms;
        send_status();
        return;
    }
    if (strcmp(line, "STOP") == 0)
    {
        g_last_valid_command = now_ms;
        servo_stop();
        (void)uart_send_line("OK STOPPED");
        return;
    }
    if (strcmp(line, "DISARM") == 0)
    {
        g_last_valid_command = now_ms;
        servo_stop();
        outputs_disable();
        (void)servo_all_off();
        g_armed = false;
        g_test_active = false;
        (void)uart_send_line("OK DISARMED");
        return;
    }
    if (strcmp(line, "ARM") == 0)
    {
        g_last_valid_command = now_ms;
#if SYSTEM_COMMISSIONED
        if (g_fault)
        {
            (void)uart_send_line("ERR FAULT");
        }
        else if (!servo_preload_home())
        {
            enter_fault("ERR PCA");
        }
        else
        {
            outputs_enable();
            g_armed = true;
            g_link_lost = false;
            (void)uart_send_line("OK ARMED");
        }
#else
        (void)uart_send_line("ERR NOT_COMMISSIONED");
#endif
        return;
    }
    if (strcmp(line, "TEST_OFF") == 0)
    {
        g_last_valid_command = now_ms;
        outputs_disable();
        if (!servo_all_off())
        {
            enter_fault("ERR PCA");
        }
        else
        {
            g_test_active = false;
            (void)uart_send_line("OK TEST_OFF");
        }
        return;
    }
    if (parse_test(line, &test_joint, &test_angle))
    {
        g_last_valid_command = now_ms;
#if CALIBRATION_MODE
        if (g_armed)
        {
            (void)uart_send_line("ERR ARMED");
        }
        else
        {
            outputs_disable();
            if (!servo_test_one(test_joint, test_angle))
            {
                enter_fault("ERR TEST_RANGE_OR_PCA");
            }
            else
            {
                outputs_enable();
                g_test_active = true;
                g_test_started_at = now_ms;
                (void)uart_send_line("OK TEST");
            }
        }
#else
        (void)uart_send_line("ERR TEST_DISABLED");
#endif
        return;
    }
    if (select_command(line))
    {
        g_last_valid_command = now_ms;
        (void)uart_send_line("OK SELECT");
        return;
    }

    if ((strcmp(line, "UP") == 0) || (strcmp(line, "DOWN") == 0))
    {
        g_last_valid_command = now_ms;
        if (!motion_allowed())
            return;
        if ((g_selected != JOINT_SHOULDER) && (g_selected != JOINT_ELBOW) &&
            (g_selected != JOINT_WRIST_PITCH))
        {
            (void)uart_send_line("ERR DIRECTION");
            return;
        }
        queue_delta((strcmp(line, "UP") == 0) ? COMMAND_STEP_DEG : -COMMAND_STEP_DEG);
        return;
    }
    if ((strcmp(line, "LEFT") == 0) || (strcmp(line, "RIGHT") == 0))
    {
        g_last_valid_command = now_ms;
        if (!motion_allowed())
            return;
        if ((g_selected != JOINT_BASE) && (g_selected != JOINT_WRIST_ROTATE))
        {
            (void)uart_send_line("ERR DIRECTION");
            return;
        }
        queue_delta((strcmp(line, "RIGHT") == 0) ? COMMAND_STEP_DEG : -COMMAND_STEP_DEG);
        return;
    }
    if ((strcmp(line, "OPEN") == 0) || (strcmp(line, "CLOSE") == 0))
    {
        g_last_valid_command = now_ms;
        if (!motion_allowed())
            return;
        if (servo_busy() && (servo_active_joint() != JOINT_GRIPPER))
        {
            (void)uart_send_line("ERR BUSY");
        }
        else if (!servo_move_absolute(JOINT_GRIPPER,
                                      (strcmp(line, "OPEN") == 0) ? 35U : 125U))
        {
            (void)uart_send_line("ERR RANGE");
        }
        else
        {
            (void)uart_send_line("OK QUEUED");
        }
        return;
    }
    if (strcmp(line, "HOME") == 0)
    {
        g_last_valid_command = now_ms;
#if SHOULDER_HOME_DIRECTION == 0
        (void)uart_send_line("ERR HOME_UNCONFIGURED");
#else
        (void)uart_send_line("ERR HOME_REQUIRES_COMMISSIONING");
#endif
        return;
    }

    (void)uart_send_line("ERR COMMAND");
}

int main(void)
{
    uint8_t byte;
    uint32_t now_ms;

    /* Disable servo outputs before enabling any peripheral or interrupt. */
    SERVO_OE_PORT |= (1U << SERVO_OE_PIN);
    SERVO_OE_DDR |= (1U << SERVO_OE_PIN);

    timebase_init();
    uart_init();
    limit_init();
    servo_init();
    sei();
    twi_init();

    if (!pca9685_init())
    {
        enter_fault("BOOT ERR PCA");
    }
    else
    {
        (void)uart_send_line("BOOT OK DISARMED");
    }

    g_last_valid_command = timebase_ms();
    wdt_enable(WDTO_2S);

    for (;;)
    {
        now_ms = timebase_ms();
        limit_poll(now_ms);
        parser_timeout(now_ms);

        if (uart_rx_error())
        {
            servo_stop();
            (void)uart_send_line("ERR UART_RX");
        }
        while (uart_read(&byte))
        {
            int result = parser_feed(byte, now_ms);
            if (result > 0)
            {
                handle_command(g_parser.text, now_ms);
            }
            else if (result < 0)
            {
                (void)uart_send_line("ERR LINE");
            }
        }

        if (g_armed && !g_link_lost &&
            ((uint32_t)(now_ms - g_last_valid_command) >= LINK_TIMEOUT_MS))
        {
            servo_stop();
            g_link_lost = true;
            (void)uart_send_line("ERR LINK_TIMEOUT HOLDING");
        }
        if (g_test_active &&
            ((uint32_t)(now_ms - g_test_started_at) >= CALIBRATION_TIMEOUT_MS))
        {
            outputs_disable();
            (void)servo_all_off();
            g_test_active = false;
            (void)uart_send_line("OK TEST_TIMEOUT_OFF");
        }

#if SHOULDER_HOME_DIRECTION != 0
        if (g_armed && limit_active() &&
            (servo_active_joint() == JOINT_SHOULDER) &&
            (servo_motion_direction() == SHOULDER_HOME_DIRECTION))
        {
            servo_stop();
            (void)uart_send_line("ERR SHOULDER_LIMIT HOLDING");
        }
#endif

        if (g_armed && !servo_tick(now_ms))
        {
            enter_fault("ERR PCA MOTION");
        }
        wdt_reset();
    }
}
