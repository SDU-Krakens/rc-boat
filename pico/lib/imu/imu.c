#include "imu.h"
#include "config.h"

#include "pico/time.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define QUAT_W 0
#define QUAT_X 1
#define QUAT_Y 2
#define QUAT_Z 3

imu_t *imu_open(imu_type_t type, i2c_t *i2c, const imu_rot_t *rot) {
  imu_t *imu = malloc(sizeof(imu_t));
  if (!imu) {
    return NULL;
  }
  imu->type = type;

  int rc = -1;
  switch (type) {
  case IMU_ICM20948: rc = icm20948_open(&imu->icm, i2c, rot); break;
  case IMU_BNO055: rc = bno055_open(&imu->bno, i2c, rot); break;
  }
  if (rc != 0) {
    free(imu);
    return NULL;
  }
  return imu;
}

void imu_close(imu_t *imu) {
  if (!imu) {
    return;
  }
  switch (imu->type) {
  case IMU_ICM20948: icm20948_close(&imu->icm); break;
  case IMU_BNO055: bno055_close(&imu->bno); break;
  }
  free(imu);
}

int imu_read(imu_t *imu, imu_sample_t *out) {
  switch (imu->type) {
  case IMU_ICM20948: return icm20948_read(&imu->icm, out);
  case IMU_BNO055: return bno055_read(&imu->bno, out);
  }
  return -1;
}

int imu_cal_gyro(imu_t *imu, uint32_t samples) {
  if (imu->type != IMU_ICM20948) {
    return -1;
  }
  float accel[3], gyro[3], mag[3], temp;
  bool mag_valid;
  uint32_t period_us = 1000000u / CONF_IMU_SAMPLE_HZ;

  calib_gyro_begin(&imu->icm.cal);
  for (uint32_t i = 0; i < samples; i++) {
    if (icm20948_read_raw(&imu->icm, accel, gyro, mag, &temp, &mag_valid) != 0) {
      return -1;
    }
    calib_gyro_update(&imu->icm.cal, gyro);
    sleep_us(period_us);
  }
  calib_gyro_end(&imu->icm.cal);
  return 0;
}

int imu_cal_mag_begin(imu_t *imu) {
  if (imu->type != IMU_ICM20948) {
    return -1;
  }
  calib_mag_begin(&imu->icm.cal);
  return 0;
}

int imu_cal_mag_update(imu_t *imu) {
  if (imu->type != IMU_ICM20948) {
    return -1;
  }
  float accel[3], gyro[3], mag[3], temp;
  bool mag_valid;
  if (icm20948_read_raw(&imu->icm, accel, gyro, mag, &temp, &mag_valid) != 0) {
    return -1;
  }
  if (mag_valid) {
    calib_mag_update(&imu->icm.cal, mag);
  }
  return 0;
}

int imu_cal_mag_end(imu_t *imu) {
  if (imu->type != IMU_ICM20948) {
    return -1;
  }
  calib_mag_end(&imu->icm.cal);
  return 0;
}

int imu_cal_save(imu_t *imu, storage_t *st) {
  switch (imu->type) {
  case IMU_ICM20948:
    return storage_save(st, IMU_STORAGE_TAG_ICM20948, &imu->icm.cal,
                        sizeof(imu->icm.cal));
  case IMU_BNO055: {
    uint8_t offsets[BNO055_OFFSETS_LEN];
    if (bno055_cal_get(&imu->bno, offsets) != 0) {
      return -1;
    }
    return storage_save(st, IMU_STORAGE_TAG_BNO055, offsets, sizeof(offsets));
  }
  }
  return -1;
}

int imu_cal_load(imu_t *imu, storage_t *st) {
  switch (imu->type) {
  case IMU_ICM20948: {
    calib_t cal;
    if (storage_load(st, IMU_STORAGE_TAG_ICM20948, &cal, sizeof(cal)) != 0) {
      return -1;
    }
    imu->icm.cal = cal;
    return 0;
  }
  case IMU_BNO055: {
    uint8_t offsets[BNO055_OFFSETS_LEN];
    if (storage_load(st, IMU_STORAGE_TAG_BNO055, offsets, sizeof(offsets)) !=
        0) {
      return -1;
    }
    return bno055_cal_set(&imu->bno, offsets);
  }
  }
  return -1;
}

void imu_average(const imu_sample_t *a, const imu_sample_t *b,
                 imu_sample_t *out) {
  for (int i = 0; i < 3; i++) {
    out->accel[i] = (a->accel[i] + b->accel[i]) / 2.0f;
    out->gyro[i] = (a->gyro[i] + b->gyro[i]) / 2.0f;
    out->mag[i] = (a->mag[i] + b->mag[i]) / 2.0f;
  }
  out->temp = (a->temp + b->temp) / 2.0f;
  out->timestamp_us =
      a->timestamp_us > b->timestamp_us ? a->timestamp_us : b->timestamp_us;

  // Raw parts only; orientation is left for the caller's filter
  uint32_t raw = IMU_VALID_ACCEL | IMU_VALID_GYRO | IMU_VALID_MAG |
                 IMU_VALID_TEMP;
  out->valid = a->valid & b->valid & raw;
}

void imu_rotate(const imu_rot_t *rot, float v[3]) {
  float r[3];
  for (int i = 0; i < 3; i++) {
    r[i] = rot->m[i][0] * v[0] + rot->m[i][1] * v[1] + rot->m[i][2] * v[2];
  }
  v[0] = r[0];
  v[1] = r[1];
  v[2] = r[2];
}

void imu_rot_to_quat(const imu_rot_t *rot, float q[4]) {
  const float(*m)[3] = rot->m;
  float trace = m[0][0] + m[1][1] + m[2][2];

  if (trace > 0.0f) {
    float s = sqrtf(trace + 1.0f) * 2.0f;
    q[QUAT_W] = 0.25f * s;
    q[QUAT_X] = (m[2][1] - m[1][2]) / s;
    q[QUAT_Y] = (m[0][2] - m[2][0]) / s;
    q[QUAT_Z] = (m[1][0] - m[0][1]) / s;
  } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
    float s = sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
    q[QUAT_W] = (m[2][1] - m[1][2]) / s;
    q[QUAT_X] = 0.25f * s;
    q[QUAT_Y] = (m[0][1] + m[1][0]) / s;
    q[QUAT_Z] = (m[0][2] + m[2][0]) / s;
  } else if (m[1][1] > m[2][2]) {
    float s = sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
    q[QUAT_W] = (m[0][2] - m[2][0]) / s;
    q[QUAT_X] = (m[0][1] + m[1][0]) / s;
    q[QUAT_Y] = 0.25f * s;
    q[QUAT_Z] = (m[1][2] + m[2][1]) / s;
  } else {
    float s = sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
    q[QUAT_W] = (m[1][0] - m[0][1]) / s;
    q[QUAT_X] = (m[0][2] + m[2][0]) / s;
    q[QUAT_Y] = (m[1][2] + m[2][1]) / s;
    q[QUAT_Z] = 0.25f * s;
  }
}

void imu_quat_mul(const float a[4], const float b[4], float out[4]) {
  float w = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
  float x = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
  float y = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
  float z = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
  out[0] = w;
  out[1] = x;
  out[2] = y;
  out[3] = z;
}

void imu_quat_to_euler(const float q[4], float *roll, float *pitch,
                       float *yaw) {
  float w = q[QUAT_W], x = q[QUAT_X], y = q[QUAT_Y], z = q[QUAT_Z];
  *roll = atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
  float s = 2.0f * (w * y - z * x);
  if (s > 1.0f) {
    s = 1.0f;
  } else if (s < -1.0f) {
    s = -1.0f;
  }
  *pitch = asinf(s);
  *yaw = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
}
