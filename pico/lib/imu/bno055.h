#pragma once

#include "../i2c/i2c.h"
#include "imu_types.h"
#include <stdint.h>

#define BNO055_ADDR_LOW 0x28
#define BNO055_ADDR_HIGH 0x29
#define BNO055_CHIP_ID_VALUE 0xA0

// Page 0 registers
#define BNO055_CHIP_ID 0x00
#define BNO055_PAGE_ID 0x07
#define BNO055_ACC_DATA 0x08 // accel, mag, gyro back to back (18 bytes)
#define BNO055_QUA_DATA 0x20
#define BNO055_TEMP 0x34
#define BNO055_CALIB_STAT 0x35
#define BNO055_UNIT_SEL 0x3B
#define BNO055_OPR_MODE 0x3D
#define BNO055_PWR_MODE 0x3E
#define BNO055_SYS_TRIGGER 0x3F
#define BNO055_OFFSETS 0x55

// Values
#define BNO055_MODE_CONFIG 0x00
#define BNO055_MODE_NDOF 0x0C
#define BNO055_PWR_NORMAL 0x00
#define BNO055_SYS_RST 0x20
#define BNO055_UNIT_GYRO_RPS 0x02 // accel m/s^2, gyro rad/s, temp C
#define BNO055_OFFSETS_LEN 22
#define BNO055_DATA_LEN 18

// Scales (datasheet, with UNIT_SEL above)
#define BNO055_ACC_LSB_PER_MS2 100.0f
#define BNO055_MAG_LSB_PER_UT 16.0f
#define BNO055_GYR_LSB_PER_RPS 900.0f
#define BNO055_QUA_LSB (1 << 14)

// CALIB_STAT fields
#define BNO055_CAL_SYS_SHIFT 6
#define BNO055_CAL_GYR_SHIFT 4
#define BNO055_CAL_ACC_SHIFT 2
#define BNO055_CAL_MAG_SHIFT 0
#define BNO055_CAL_MASK 0x03

// Delays
#define BNO055_RESET_MS 650
#define BNO055_TO_CONFIG_MS 19
#define BNO055_FROM_CONFIG_MS 7

typedef struct {
  i2c_t *i2c;
  uint8_t addr;
  imu_rot_t rot;
} bno055_t;

int bno055_open(bno055_t *bno, i2c_t *i2c, const imu_rot_t *rot);
void bno055_close(bno055_t *bno);
int bno055_read(bno055_t *bno, imu_sample_t *out);
int bno055_cal_get(bno055_t *bno, uint8_t offsets[22]);
int bno055_cal_set(bno055_t *bno, const uint8_t offsets[22]);
