
/*
 * date: 2025-09-28
 * author: ps
 * title: library for the use of the imu mpu-6050 with raspberry pi in c code
 */
#pragma once

// includes vv
#include <stdio.h>
#include <stdlib.h>
// #include <sys/types.h>
#include <stdint.h>
// defines vv
#define adr_mpu 0x68 // specified by the breakout board we
#define gyro_range                                                             \
	0 // select which gyroscope range to use (see the table below) - default
	  // is 0
//	gyroscope range
//	0	+/- 250 degrees/second
//	1	+/- 500 degrees/second
//	2	+/- 1000 degrees/second
//	3	+/- 2000 degrees/second
#define accel_range                                                            \
	0 // select which accelerometer range to use (see the table below) -
	  // default is 0
//	accelerometer range
//	0	+/- 2g
//	1	+/- 4g
//	2	+/- 8g
//	3	+/- 16g
// see the mpu6000 register map for more information

// function declatations  vv
int i2c_read(char adr_slave,
	     char adr_register); // returns the register value as a decimal
void getrawacc(int *xx, int *yy, int *zz);
void imuconfig();
int i2c_write(char adr_slave, char adr_register, char data);

// global variables vv
// /* defines the rang of values the gyro will suply.
//  * the allowed values are in +- degrees/second:
//  * 250
//  * 500
//  * 1000
//  * 2000
//  */
u_int8_t gyro_scale = 250; // [+- deg/s]
//
// /* defines the range of values the accelerometer will suply.
//  * the allowed values are in +- g (multiplies of ~9.81m/s/s):
//  * 2
//  * 4
//  * 8
//  * 16
//  */
u_int8_t accelerometer_scale = 2; // [+- g]

u_int8_t dlpf = 0; // 3bit // changes depending on the bandwidth and delay of
		   // gyro and accelerometer (see register map page 13)
u_int8_t ext_synq = 0;	   // 3bit // no syncing
u_int8_t sample_rate = 10; // [khz]
