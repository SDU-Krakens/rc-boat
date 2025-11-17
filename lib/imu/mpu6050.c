#include "mpu6050.h"
// #include <cstdint>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

// int main() {
//   int test = i2c_read(adr_mpu, 0x26);
//   imuConfig();
//   printf("test: %d\n", test);
//   float x, y, z;
//   getRawAcc(&x, &y, &z);
//   printf("\n%f, %f, %f", x, y, z);
//   i2c_write(adr_mpu, 0x26, 0xaa);
//   printf("\n%d", i2c_read(adr_mpu, 0x26));
//   printf("\n\n");
//   return 0;
// }

// global variables vv
// /* Defines the rang of values the gyro will suply.
//  * The allowed values are in +- degrees/second:
//  * 250
//  * 500
//  * 1000
//  * 2000
//  */
uint8_t gyro_scale; // [+- deg/s]
                    //
// /* Defines the range of values the accelerometer will suply.
//  * The allowed values are in +- g (multiplies of ~9.81m/s/s):
//  * 2
//  * 4
//  * 8
//  * 16
//  */
uint8_t accelerometerScale; // [+- g]

uint8_t dlpf;        // 3bit // changes depending on the bandwidth and delay of
                     // gyro and accelerometer (see register map page 13)
uint8_t ext_synq;    // 3bit // no syncing
uint8_t sample_rate; // [kHz]

int i2c_read(char adr_slave, char adr_register) {
  int result = -1;
  char *string;
  if (0 > asprintf(&string, "i2cget -y 1 %d %d", adr_slave, adr_register))
    return -1;
  FILE *pipe;
  char buffer[50];
  char *buff_ret; // buffer return value
  pipe = popen(string, "r");
  free(string);
  if (pipe == NULL) {
    puts("Unable to open process");
    return (-1);
  }
  buff_ret = fgets(buffer, sizeof(buffer), pipe);
  if (buff_ret == NULL) {
    return -1;
  } else {
    sscanf(buffer, "%x", &result);
  }
  int status = pclose(pipe);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return -1;
  }
  return result;
}
int i2c_write(char adr_slave, char adr_register, char data) {
  char *string;
  if (0 >
      asprintf(&string, "i2cset -y 1 %d %d %d", adr_slave, adr_register, data))
    return -1;
  FILE *pipe;
  pipe = popen(string, "r");
  free(string);
  if (pipe == NULL) {
    puts("Unable to open process");
    return (-1);
  }
  int status = pclose(pipe);
  return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}
void getRawAcc(int *xx, int *yy, int *zz) {
  uint16_t x = (i2c_read(0x68, 0x3b) << 8); // read high byte
  x |= i2c_read(0x68, 0x3C);                // read low byte and add to high one
  uint16_t y = (i2c_read(0x68, 0x3D) << 8);
  y |= i2c_read(0x68, 0x3E);
  uint16_t z = (i2c_read(0x68, 0x3F) << 8);
  z |= i2c_read(0x68, 0x40);
  *xx = (int)x; // change into int and return
  *yy = (int)y;
  *zz = (int)z;
}
void imuConfig() {
  //    uint8t pwr_mgmt_1 = i2cread(adr, 0x6B); // read the power
  //    management byte i2cwrite(adr_mpu, 0x6b, (pwr_mgmt_1&(~(1<<6))));
  //    // write back the byte with sleep disabled
  i2c_write(adr_mpu, 0x6b, 0x00); // turn off sleep mode
  uint8_t gyro_out_rate;
  if (dlpf == 0 || dlpf == 7) { // decide what the GOR is
    gyro_out_rate = 8;
  } else {
    gyro_out_rate = 1;
  }
  uint8_t samplediv = gyro_out_rate / sample_rate - 1; // calculate SD
  i2c_write(adr_mpu, 0x19, samplediv);                 // send SD to MPU
  uint8_t config_val = 0x00 | dlpf | (ext_synq << 3);
  i2c_write(adr_mpu, 0x1a, config_val); // external synq and dlpf
  i2c_write(adr_mpu, 0x1b,
            GYRO_RANGE << 3); // sets up the range of gyroscope
  i2c_write(adr_mpu, 0x1c, ACCEL_RANGE << 3);
}

void getRawGyro(int *roll, int *pitch, int *yaw) {
  uint16_t x = (i2c_read(adr_mpu, 0x43) << 8); // read high byte
  x |= i2c_read(adr_mpu, 0x44); // read low byte and add to high one
  uint16_t y = (i2c_read(adr_mpu, 0x45) << 8);
  y |= i2c_read(adr_mpu, 0x46);
  uint16_t z = (i2c_read(adr_mpu, 0x47) << 8);
  z |= i2c_read(adr_mpu, 0x48);
  *roll = (int)x; // change into int and return
  *pitch = (int)y;
  *yaw = (int)z;
}
void get_unfil_gyro(
    float *roll, float *pitch,
    float *yaw) { // fills the provided float pointers with unfiltered
                  // angular speed in deg/s using the gyro_scale
  int x, y, z;
  getRawGyro(&x, &y, &z);
  *roll = (((float)x - (UINT16_MAX / 2)) * gyro_scale) / (UINT16_MAX / 2);
  *pitch = (((float)y - (UINT16_MAX / 2)) * gyro_scale) / (UINT16_MAX / 2);
  *yaw = (((float)z - (UINT16_MAX / 2)) * gyro_scale) / (UINT16_MAX / 2);
}
