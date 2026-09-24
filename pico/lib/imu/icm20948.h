#pragma once

#include "../i2c/i2c.h"
#include "calib.h"
#include "imu_types.h"
#include "madgwick.h"
#include <stdbool.h>
#include <stdint.h>

#define ICM20948_ADDR_LOW 0x68
#define ICM20948_ADDR_HIGH 0x69
#define ICM20948_WHO_AM_I_VALUE 0xEA

// Register bank select (all banks)
#define ICM20948_REG_BANK_SEL 0x7F
#define ICM20948_BANK_SHIFT 4

// Bank 0
#define ICM20948_WHO_AM_I 0x00
#define ICM20948_USER_CTRL 0x03
#define ICM20948_PWR_MGMT_1 0x06
#define ICM20948_PWR_MGMT_2 0x07
#define ICM20948_I2C_MST_STATUS 0x17
#define ICM20948_ACCEL_XOUT_H 0x2D
#define ICM20948_EXT_SLV_SENS_DATA_00 0x3B

// Bank 2
#define ICM20948_GYRO_SMPLRT_DIV 0x00
#define ICM20948_GYRO_CONFIG_1 0x01
#define ICM20948_ACCEL_SMPLRT_DIV_1 0x10
#define ICM20948_ACCEL_SMPLRT_DIV_2 0x11
#define ICM20948_ACCEL_CONFIG 0x14

// Bank 3
#define ICM20948_I2C_MST_CTRL 0x01
#define ICM20948_I2C_MST_DELAY_CTRL 0x02
#define ICM20948_I2C_SLV0_ADDR 0x03
#define ICM20948_I2C_SLV0_REG 0x04
#define ICM20948_I2C_SLV0_CTRL 0x05
#define ICM20948_I2C_SLV4_ADDR 0x13
#define ICM20948_I2C_SLV4_REG 0x14
#define ICM20948_I2C_SLV4_CTRL 0x15
#define ICM20948_I2C_SLV4_DO 0x16
#define ICM20948_I2C_SLV4_DI 0x17

// Bits
#define ICM20948_PWR_RESET 0x80
#define ICM20948_PWR_CLK_AUTO 0x01
#define ICM20948_PWR_ALL_ON 0x00
#define ICM20948_USER_I2C_MST_EN 0x20
#define ICM20948_USER_I2C_MST_RST 0x02
#define ICM20948_MST_CLK_345KHZ 0x07
#define ICM20948_MST_P_NSR 0x10
#define ICM20948_SLV_READ 0x80
#define ICM20948_SLV_EN 0x80
#define ICM20948_SLV0_DELAY_EN 0x01
#define ICM20948_SLV4_DONE 0x40
#define ICM20948_FCHOICE 0x01
#define ICM20948_FS_SHIFT 1
#define ICM20948_DLPF_SHIFT 3
#define ICM20948_DLPF_MAX 7

// Internal sample rates
#define ICM20948_GYRO_BASE_HZ 1100.0f
#define ICM20948_ACCEL_BASE_HZ 1125.0f
#define ICM20948_TEMP_SENS 333.87f
#define ICM20948_TEMP_OFFSET 21.0f

// Access the mag every (1 + ICM20948_MAG_READ_DLY) samples so each read
// finds new data (mag runs at 100 Hz)
#define ICM20948_MAG_READ_DLY 1

// AK09916 magnetometer (behind the ICM's I2C master)
#define AK09916_ADDR 0x0C
#define AK09916_WIA2 0x01
#define AK09916_WIA2_VALUE 0x09
#define AK09916_ST1 0x10
#define AK09916_CNTL2 0x31
#define AK09916_CNTL3 0x32
#define AK09916_ST1_DRDY 0x01
#define AK09916_ST2_HOFL 0x08
#define AK09916_MODE_CONT_100HZ 0x08
#define AK09916_SOFT_RESET 0x01
#define AK09916_READ_LEN 9 // ST1, HXL..HZH, TMPS, ST2
#define AK09916_UT_PER_LSB 0.15f

// Burst read: accel (6) + gyro (6) + temp (2) + mag (9)
#define ICM20948_BURST_LEN (14 + AK09916_READ_LEN)

// Delays
#define ICM20948_RESET_MS 100
#define ICM20948_WAKE_MS 10
#define ICM20948_SLV4_TIMEOUT_US 10000
#define ICM20948_MAG_RESET_MS 10

typedef struct {
  i2c_t *i2c;
  uint8_t addr;
  imu_rot_t rot;
  float accel_scale, gyro_scale;
  calib_t cal;
  madgwick_t filter;
  uint64_t last_us;
} icm20948_t;

int icm20948_open(icm20948_t *icm, i2c_t *i2c, const imu_rot_t *rot);
void icm20948_close(icm20948_t *icm);
int icm20948_read(icm20948_t *icm, imu_sample_t *out);

// Uncalibrated readings in the chip frame (accel m/s^2, gyro rad/s, mag uT),
// used for calibration
int icm20948_read_raw(icm20948_t *icm, float accel[3], float gyro[3],
                      float mag[3], float *temp, bool *mag_valid);
