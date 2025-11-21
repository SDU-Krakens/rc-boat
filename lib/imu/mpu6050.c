// Wrapper for i2c communication with IMU
// dependencies:
//    - needs i2c-tools package (version? 1.69~deb13u1 arm64)
// date: 2025-11-21
// author: PS
//

#include "mpu6050.h"
#include "../i2c/i2c.h"
#include <stdio.h>

// Variable definitions (initialized in source file)
uint8_t gyro_scale = 250;        // [+- deg/s]
uint8_t accelerometer_scale = 2; // [+- g]
uint8_t dlpf = 0;     // 3bit // changes depending on the bandwidth and delay of
                      // gyro and accelerometer (see register map page 13)
uint8_t ext_synq = 0; // 3bit // no syncing
uint8_t sample_rate = 10; // [khz]
uint8_t wait_for_es =
    1; // 1bit // if 1: data ready interrupt waits for the slave data to arrive
uint8_t i2c_mst_clk = 0; // 4bit // determins the clock frequency of the i2c
                         // master (see register map page 18)

void imu_config() {
  //    uint8t pwr_mgmt_1 = i2cread(adr, 0x6B); // read the power
  //    management byte i2cwrite(adr_mpu, 0x6b, (pwr_mgmt_1&(~(1<<6))));
  //    // write back the byte with sleep disabled
  i2c_write(adr_mpu, 0x6b, 0x00); // turn off sleep mode
  u_int8_t gyro_out_rate;
  if (dlpf == 0 || dlpf == 7) { // decide what the GOR is
    gyro_out_rate = 8;
  } else {
    gyro_out_rate = 1;
  }
  u_int8_t samplediv = gyro_out_rate / sample_rate - 1; // calculate SD
  i2c_write(adr_mpu, 0x19, samplediv);                  // send SD to MPU
  u_int8_t config_val = 0x00 | dlpf | (ext_synq << 3);
  i2c_write(adr_mpu, 0x1a, config_val); // external synq and dlpf
  i2c_write(adr_mpu, 0x1b,
            gyro_range << 3); // sets up the range of gyroscope
  i2c_write(adr_mpu, 0x1c,
            accel_range << 3); // sets up accelerometer range
                               // i2c master configuration vv
  u_int8_t maseter_conf =
      0x00 | (wait_for_es << 6) |
      i2c_mst_clk; // configurations for the sub master i2c controll
  i2c_write(adr_mpu, 0x24, maseter_conf);
  i2c_write(adr_mpu, 0x25,
            (adr_mpu >> 1)); // defines the adress of the first sub slave
}
void get_raw_acc(int *xx, int *yy, int *zz) {
  u_int16_t x = (i2c_read(adr_mpu, 0x3b) << 8); // read high byte
  x |= i2c_read(adr_mpu, 0x3C); // read low byte and add to high one
  u_int16_t y = (i2c_read(adr_mpu, 0x3D) << 8);
  y |= i2c_read(adr_mpu, 0x3E);
  u_int16_t z = (i2c_read(adr_mpu, 0x3F) << 8);
  z |= i2c_read(adr_mpu, 0x40);
  *xx = (int)x; // change into int and return
  *yy = (int)y;
  *zz = (int)z;
}
void get_raw_gyro(int *roll, int *pitch, int *yaw) {
  u_int16_t x = (i2c_read(adr_mpu, 0x43) << 8); // read high byte
  x |= i2c_read(adr_mpu, 0x44); // read low byte and add to high one
  u_int16_t y = (i2c_read(adr_mpu, 0x45) << 8);
  y |= i2c_read(adr_mpu, 0x46);
  u_int16_t z = (i2c_read(adr_mpu, 0x47) << 8);
  z |= i2c_read(adr_mpu, 0x48);
  *roll = (int)x; // change into int and return
  *pitch = (int)y;
  *yaw = (int)z;
}
void get_unfil_gyro(
    float *roll, float *pitch,
    float *yaw) { // fills the provided float pointers with unfiltered
                  // angular speed in deg/s using the gyro_scale
  int x, y, z;    // MIGHT NEED TO BE UNSIGNED
  get_raw_gyro(&x, &y, &z);
  *roll = (((float)x - (UINT16_MAX / 2)) * gyro_scale) / (UINT16_MAX / 2);
  *pitch = (((float)y - (UINT16_MAX / 2)) * gyro_scale) / (UINT16_MAX / 2);
  *yaw = (((float)z - (UINT16_MAX / 2)) * gyro_scale) / (UINT16_MAX / 2);
}
void get_unfil_acc(
    float *x, float *y,
    float *z) {   // fills the provided float pointers with
                  // unfiltered acceleration in g (multiplications
                  // of earth acceleration) using the accelerometer_range
  int xx, yy, zz; // MIGHT NEED TO BE UNSIGNED
  get_raw_acc(&xx, &yy, &zz);
  *x =
      (((float)xx - (UINT16_MAX / 2)) * accelerometer_scale) / (UINT16_MAX / 2);
  *y =
      (((float)yy - (UINT16_MAX / 2)) * accelerometer_scale) / (UINT16_MAX / 2);
  *z =
      (((float)zz - (UINT16_MAX / 2)) * accelerometer_scale) / (UINT16_MAX / 2);
}
void write_to_slave0(u_int8_t adr_register, u_int8_t data) {
  i2c_write(adr_mpu, 0x26, adr_register); // defines which register on
                                          // slave will be written into
  i2c_write(
      adr_mpu, 0x25,
      ((0 << 7) | (adr_slv0 >> 1))); // defines the operation as writing and
                                     // the adress as the slave 0 adress
  u_int8_t length = 0;               // 4bit // how many bytes will be sent
  u_int8_t slave_conf = length | (1 << 7);
  i2c_write(adr_mpu, 0x63,
            data); // determins what dataw ill be written into the sub slave
  i2c_write(adr_mpu, 0x27,
            slave_conf); // enables writing 1 byte to sub slave 0
  while (i2c_read(adr_mpu, 0x27) &
         (1 << 7)) { // checks if the data send enable bit is still up
    printf(".");     // debuging shit, delete later please!!!!
  }
}
void write_to_slave(u_int8_t adr_slave, u_int8_t adr_register, u_int8_t data) {
  u_int8_t write_adress, slv_adr_adr, slv_reg_adr, slv_data_adr, slv_conf_adr;
  switch (adr_slave) {
  case adr_slv0:
    write_adress = (0 << 7) | (adr_slv0 >> 1);
    slv_adr_adr = 0x25;
    slv_reg_adr = 0x26;
    slv_data_adr = 0x63;
    slv_conf_adr = 0x27;
    break;
  default:
    write_adress = (0 << 7) | (adr_slv0 >> 1);
    slv_adr_adr = 0x25;
    slv_reg_adr = 0x26;
    slv_data_adr = 0x63;
    slv_conf_adr = 0x27;
    break;
  }
  i2c_write(adr_mpu, slv_reg_adr,
            adr_register); // defines which register on
                           // slave will be written into
  i2c_write(adr_mpu, slv_adr_adr,
            write_adress); // defines the operation as writing and
                           // the adress as the slave 0 adress
  u_int8_t length = 0;     // 4bit // how many bytes will be sent
  u_int8_t slave_conf = length | (1 << 7);
  i2c_write(adr_mpu, slv_data_adr,
            data); // determins what dataw ill be written into the sub slave
  i2c_write(adr_mpu, slv_conf_adr,
            slave_conf); // enables writing 1 byte to sub slave 0
  while (i2c_read(adr_mpu, slv_conf_adr) &
         (1 << 7)) { // checks if the data send enable bit is still up
    printf(".");     // debuging shit, delete later please!!!!
  }
}

// void selfTest(){
//   str = out_without_TS - out_with_TS
// }
