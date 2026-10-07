#include "servo.h"
#include "config.h"
#include "pca9685.h"

typedef struct
{
    uint8_t channel;
    uint8_t enabled;
    uint8_t min_angle;
    uint8_t max_angle;
    uint8_t home_angle;
    uint16_t min_us;
    uint16_t max_us;
    uint8_t invert;
    uint8_t current;
    uint8_t target;
} servo_axis_t;

/*
 * Conservative commissioning values. They are not mechanical truth.
 * Change min/max pulse, angle and invert only after unloaded measurement.
 */
static servo_axis_t g_axis[JOINT_COUNT] =
{
    /* channel, enabled, min, max, home, min_us, max_us, invert, current, target */
    {0U, 1U, 20U, 160U, 90U, 1000U, 2000U, 0U, 90U, 90U}, /* Base: MG995 */
    {1U, 1U, 25U, 150U, 90U, 1000U, 2000U, 0U, 90U, 90U}, /* Shoulder: MG996 */
    {2U, 1U, 20U, 160U, 90U, 1000U, 2000U, 0U, 90U, 90U}, /* Elbow: MG996 */
    {3U, 1U, 20U, 160U, 90U, 1000U, 2000U, 0U, 90U, 90U}, /* Wrist pitch: SG90 */
    {4U, 0U, 15U, 165U, 90U, 1000U, 2000U, 0U, 90U, 90U}, /* Wrist rotate removed */
    {5U, 1U, 25U, 135U, 35U, 1000U, 2000U, 0U, 35U, 35U}  /* Gripper: SG90 */
};

static joint_t g_active = JOINT_NONE;
static uint32_t g_last_step;

static uint16_t angle_to_us(const servo_axis_t *axis, uint8_t angle)
{
    uint8_t logical = angle;
    uint16_t span_angle = (uint16_t)(axis->max_angle - axis->min_angle);
    uint16_t span_us = (uint16_t)(axis->max_us - axis->min_us);

    if (axis->invert != 0U)
    {
        logical = (uint8_t)(axis->max_angle - (angle - axis->min_angle));
    }
    return (uint16_t)(axis->min_us +
                      ((uint32_t)(logical - axis->min_angle) * span_us +
                       (span_angle / 2U)) / span_angle);
}

static bool write_angle(joint_t joint, uint8_t angle)
{
    servo_axis_t *axis;
    if ((uint8_t)joint >= JOINT_COUNT)
    {
        return false;
    }
    axis = &g_axis[(uint8_t)joint];
    if (axis->enabled == 0U)
    {
        return false;
    }
    return pca9685_set_pulse_us(axis->channel, angle_to_us(axis, angle));
}

void servo_init(void)
{
    uint8_t index;
    g_active = JOINT_NONE;
    g_last_step = 0UL;
    for (index = 0U; index < JOINT_COUNT; ++index)
    {
        g_axis[index].current = g_axis[index].home_angle;
        g_axis[index].target = g_axis[index].home_angle;
    }
}

bool servo_preload_home(void)
{
    uint8_t index;
    for (index = 0U; index < JOINT_COUNT; ++index)
    {
        g_axis[index].current = g_axis[index].home_angle;
        g_axis[index].target = g_axis[index].home_angle;
        if (g_axis[index].enabled == 0U)
        {
            if (!pca9685_channel_off(g_axis[index].channel))
            {
                return false;
            }
            continue;
        }
        if (!write_angle((joint_t)index, g_axis[index].home_angle))
        {
            return false;
        }
    }
    g_active = JOINT_NONE;
    return true;
}

bool servo_move_absolute(joint_t joint, uint8_t angle)
{
    servo_axis_t *axis;
    if ((uint8_t)joint >= JOINT_COUNT)
    {
        return false;
    }
    axis = &g_axis[(uint8_t)joint];
    if (axis->enabled == 0U)
    {
        return false;
    }
    if ((angle < axis->min_angle) || (angle > axis->max_angle))
    {
        return false;
    }
    if ((g_active != JOINT_NONE) && (g_active != joint))
    {
        return false;
    }
    axis->target = angle;
    g_active = (axis->target == axis->current) ? JOINT_NONE : joint;
    return true;
}

bool servo_move_delta(joint_t joint, int8_t delta_deg)
{
    int16_t requested;
    if ((uint8_t)joint >= JOINT_COUNT)
    {
        return false;
    }
    requested = (int16_t)g_axis[(uint8_t)joint].target + delta_deg;
    if ((requested < 0) || (requested > 180))
    {
        return false;
    }
    return servo_move_absolute(joint, (uint8_t)requested);
}

bool servo_tick(uint32_t now_ms)
{
    servo_axis_t *axis;
    int8_t direction;
    uint8_t next;

    if (g_active == JOINT_NONE)
    {
        return true;
    }
    if ((uint32_t)(now_ms - g_last_step) < MOTION_PERIOD_MS)
    {
        return true;
    }
    g_last_step = now_ms;
    axis = &g_axis[(uint8_t)g_active];
    direction = (axis->target > axis->current) ? MOTION_STEP_DEG : -MOTION_STEP_DEG;
    next = (uint8_t)((int16_t)axis->current + direction);

    if (!write_angle(g_active, next))
    {
        return false;
    }
    axis->current = next;
    if (axis->current == axis->target)
    {
        g_active = JOINT_NONE;
    }
    return true;
}

void servo_stop(void)
{
    uint8_t index;
    for (index = 0U; index < JOINT_COUNT; ++index)
    {
        g_axis[index].target = g_axis[index].current;
    }
    g_active = JOINT_NONE;
}

bool servo_busy(void)
{
    return g_active != JOINT_NONE;
}

joint_t servo_active_joint(void)
{
    return g_active;
}

int8_t servo_motion_direction(void)
{
    const servo_axis_t *axis;
    if (g_active == JOINT_NONE)
    {
        return 0;
    }
    axis = &g_axis[(uint8_t)g_active];
    return (axis->target > axis->current) ? 1 : -1;
}

uint8_t servo_angle(joint_t joint)
{
    return ((uint8_t)joint < JOINT_COUNT) ? g_axis[(uint8_t)joint].current : 0U;
}

uint8_t servo_target(joint_t joint)
{
    return ((uint8_t)joint < JOINT_COUNT) ? g_axis[(uint8_t)joint].target : 0U;
}

bool servo_test_one(joint_t joint, uint8_t angle)
{
    servo_axis_t *axis;
    if ((uint8_t)joint >= JOINT_COUNT)
    {
        return false;
    }
    axis = &g_axis[(uint8_t)joint];
    if (axis->enabled == 0U)
    {
        return false;
    }
    if ((angle < axis->min_angle) || (angle > axis->max_angle))
    {
        return false;
    }
    if (!pca9685_all_off() || !write_angle(joint, angle))
    {
        return false;
    }
    axis->current = angle;
    axis->target = angle;
    g_active = JOINT_NONE;
    return true;
}

bool servo_all_off(void)
{
    servo_stop();
    return pca9685_all_off();
}

