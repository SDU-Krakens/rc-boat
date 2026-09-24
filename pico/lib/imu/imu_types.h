#pragma once

#include <stdint.h>

// Rotation from chip frame to boat frame: v_boat = m * v_chip
typedef struct {
  float m[3][3];
} imu_rot_t;

#define IMU_ROT_IDENTITY ((imu_rot_t){{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}})

#define IMU_VALID_ACCEL (1u << 0)
#define IMU_VALID_GYRO (1u << 1)
#define IMU_VALID_MAG (1u << 2)
#define IMU_VALID_TEMP (1u << 3)
#define IMU_VALID_ORIENT (1u << 4)

// Calibration level per part, 0 (none) .. 3 (fully calibrated)
#define IMU_CAL_NONE 0
#define IMU_CAL_FULL 3

typedef struct {
  uint8_t sys, accel, gyro, mag;
} imu_cal_status_t;

typedef struct {
  uint64_t timestamp_us;
  float accel[3];         // m/s^2, boat frame
  float gyro[3];          // rad/s
  float mag[3];           // uT
  float temp;             // deg C
  float quat[4];          // w, x, y, z
  float roll, pitch, yaw; // rad
  uint32_t valid;         // IMU_VALID_*
  imu_cal_status_t cal;
} imu_sample_t;

// Helpers shared by the drivers
void imu_rotate(const imu_rot_t *rot, float v[3]);
void imu_rot_to_quat(const imu_rot_t *rot, float q[4]);
void imu_quat_mul(const float a[4], const float b[4], float out[4]);
void imu_quat_to_euler(const float q[4], float *roll, float *pitch, float *yaw);
