#pragma once

#include <stdbool.h>
#include <stdint.h>

// IMU sample

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

// GPS fix

typedef struct {
  int32_t lat, lon;    // 1e-7 deg
  int32_t height_mm;   // above ellipsoid
  int32_t gspeed_mm_s;
  int32_t heading_1e5; // 1e-5 deg
  uint8_t fix_type, num_sv;
  uint16_t year;
  uint8_t month, day, hour, min, sec;
  uint8_t valid; // NAV-PVT valid flags
} gps_fix_t;

// CAN frame

#define CAN_MAX_DLC 8

typedef struct {
  uint32_t id;
  bool ext;
  bool rtr;
  uint8_t dlc;
  uint8_t data[CAN_MAX_DLC];
} can_frame_t;
