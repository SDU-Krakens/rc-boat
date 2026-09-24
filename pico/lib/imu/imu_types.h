#pragma once

#include "types.h"

#include <stdint.h>

// Rotation from chip frame to boat frame: v_boat = m * v_chip
typedef struct {
  float m[3][3];
} imu_rot_t;

#define IMU_ROT_IDENTITY ((imu_rot_t){{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}})

// Helpers shared by the drivers
void imu_rotate(const imu_rot_t *rot, float v[3]);
void imu_rot_to_quat(const imu_rot_t *rot, float q[4]);
void imu_quat_mul(const float a[4], const float b[4], float out[4]);
void imu_quat_to_euler(const float q[4], float *roll, float *pitch, float *yaw);
