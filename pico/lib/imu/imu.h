#pragma once

#include "../i2c/i2c.h"
#include "../storage/storage.h"
#include "bno055.h"
#include "icm20948.h"
#include "imu_types.h"
#include <stdint.h>

// Storage tags for the calibration blobs
#define IMU_STORAGE_TAG_ICM20948 1
#define IMU_STORAGE_TAG_BNO055 2

typedef enum { IMU_ICM20948, IMU_BNO055 } imu_type_t;

typedef struct {
  imu_type_t type;
  union {
    icm20948_t icm;
    bno055_t bno;
  };
} imu_t;

imu_t *imu_open(imu_type_t type, i2c_t *i2c, const imu_rot_t *rot);
void imu_close(imu_t *imu);
int imu_read(imu_t *imu, imu_sample_t *out);
int imu_cal_gyro(imu_t *imu, uint32_t samples);
int imu_cal_gyro_begin(imu_t *imu, uint32_t samples);
int imu_cal_gyro_update(imu_t *imu); // 1 done, 0 collecting, -1 error
int imu_set_rate(imu_t *imu, uint32_t hz);
int imu_set_beta(imu_t *imu, float beta);
int imu_cal_mag_begin(imu_t *imu);
int imu_cal_mag_update(imu_t *imu);
int imu_cal_mag_end(imu_t *imu);
int imu_cal_save(imu_t *imu, storage_t *st);
int imu_cal_load(imu_t *imu, storage_t *st);
void imu_average(const imu_sample_t *a, const imu_sample_t *b,
                 imu_sample_t *out);
