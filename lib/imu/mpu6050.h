/*
 * date: 2025-09-28
 * author: PS
 * title: Library for the use of the IMU MPU-6050 with raspberry PI in C code
 */
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MPU6050_ADDR 0x68

#define GYRO_RANGE 0
//	Gyroscope Range
//	0	+/- 250 degrees/second
//	1	+/- 500 degrees/second
//	2	+/- 1000 degrees/second
//	3	+/- 2000 degrees/second

#define ACCEL_RANGE 0
//	Accelerometer Range
//	0	+/- 2g
//	1	+/- 4g
//	2	+/- 8g
//	3	+/- 16g

// Sensitivity scale factors (from MPU-6050 datasheet)
static const float GYRO_SCALE_FACTOR[] = {250.0f, 500.0f, 1000.0f, 2000.0f};
static const float ACCEL_SCALE_FACTOR[] = {2.0f, 4.0f, 8.0f, 16.0f};

int i2c_read(uint8_t adr_slave, uint8_t adr_register);
int i2c_write(uint8_t adr_slave, uint8_t adr_register, uint8_t data);
void imuConfig(void);
void getRawAcc(int16_t *xx, int16_t *yy, int16_t *zz);
void getRawGyro(int16_t *roll, int16_t *pitch, int16_t *yaw);
void getGyro(float *roll, float *pitch, float *yaw);
void getAccel(float *x, float *y, float *z);
