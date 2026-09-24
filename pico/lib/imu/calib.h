#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  float gyro_bias[3];
  float mag_offset[3];
  float mag_scale[3];
  float acc_sum[3]; // gyro collection state
  uint32_t n;
  float mag_min[3], mag_max[3]; // mag collection state
} calib_t;

void calib_open(calib_t *cal);
void calib_gyro_begin(calib_t *cal);
void calib_gyro_update(calib_t *cal, const float gyro[3]);
void calib_gyro_end(calib_t *cal);
void calib_mag_begin(calib_t *cal);
void calib_mag_update(calib_t *cal, const float mag[3]);
void calib_mag_end(calib_t *cal);
void calib_apply(const calib_t *cal, float gyro[3], float mag[3]);
