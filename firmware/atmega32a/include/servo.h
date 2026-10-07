#ifndef SERVO_H
#define SERVO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    JOINT_BASE = 0,
    JOINT_SHOULDER,
    JOINT_ELBOW,
    JOINT_WRIST_PITCH,
    JOINT_WRIST_ROTATE,
    JOINT_GRIPPER,
    JOINT_NONE = 0xFF
} joint_t;

void servo_init(void);
bool servo_preload_home(void);
bool servo_move_delta(joint_t joint, int8_t delta_deg);
bool servo_move_absolute(joint_t joint, uint8_t angle);
bool servo_tick(uint32_t now_ms);
void servo_stop(void);
bool servo_busy(void);
joint_t servo_active_joint(void);
int8_t servo_motion_direction(void);
uint8_t servo_angle(joint_t joint);
uint8_t servo_target(joint_t joint);
bool servo_test_one(joint_t joint, uint8_t angle);
bool servo_all_off(void);

#endif

