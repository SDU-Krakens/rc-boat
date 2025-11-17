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
#define GYRO_RANGE 0 //Select which gyroscope range to use (see the table below) - Default is 0
//	Gyroscope Range
//	0	+/- 250 degrees/second
//	1	+/- 500 degrees/second
//	2	+/- 1000 degrees/second
//	3	+/- 2000 degrees/second
#define ACCEL_RANGE 0 //Select which accelerometer range to use (see the table below) - Default is 0
//	Accelerometer Range
//	0	+/- 2g
//	1	+/- 4g
//	2	+/- 8g
//	3	+/- 16g
//See the MPU6000 Register Map for more information

// global variables vv
// /* Defines the rang of values the gyro will suply.
//  * The allowed values are in +- degrees/second:
//  * 250
//  * 500
//  * 1000
//  * 2000
//  */
// u_int8_t gyroScale = 250; // [+- deg/s]
// 
// /* Defines the range of values the accelerometer will suply.
//  * The allowed values are in +- g (multiplies of ~9.81m/s/s):
//  * 2
//  * 4
//  * 8
//  * 16
//  */
// u_int8_t accelerometerScale = 2; // [+- g]


u_int8_t dlpf = 0; // 3bit // changes depending on the bandwidth and delay of gyro and accelerometer (see register map page 13)
u_int8_t ext_synq = 0; // 3bit // no syncing 
u_int8_t sample_rate = 10; // [kHz]



// function declatations  vv
