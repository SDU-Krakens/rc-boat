/*
 * date: 2025-09-28
 * author: PS
 * title: Library for the use of the IMU MPU-6050 with raspberry PI in C code
 */
#pragma once

// includes vv
#include <stdio.h>
#include <stdlib.h>
// defines vv
#define adr_mpu 0x68 // specified by the breakout board we
#define GYRO_RANGE                                                             \
  0 // Select which gyroscope range to use (see the table below) - Default
    // is 0
//	Gyroscope Range
//	0	+/- 250 degrees/second
//	1	+/- 500 degrees/second
//	2	+/- 1000 degrees/second
//	3	+/- 2000 degrees/second
#define ACCEL_RANGE                                                            \
  0 // Select which accelerometer range to use (see the table below) -
    // Default is 0
    //	Accelerometer Range
    //	0	+/- 2g
    //	1	+/- 4g
    //	2	+/- 8g
    //	3	+/- 16g
    // See the MPU6000 Register Map for more information

int i2c_read(char adr_slave, char adr_register);
void getRawAcc(int *xx, int *yy, int *zz);
void getRawGyro(int *roll, int *pitch, int *yaw);
void imuConfig();
int i2c_write(char adr_slave, char adr_register, char data);
void get_unfil_gyro(float *roll, float *pitch, float *yaw);
