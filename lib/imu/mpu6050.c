#include "mpu6050.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int i2c_read(uint8_t adr_slave, uint8_t adr_register) {
  int result = -1;
  char *string;
  if (0 > asprintf(&string, "i2cget -y 1 0x%02x 0x%02x", adr_slave, adr_register))
    return -1;
  FILE *pipe = popen(string, "r");
  free(string);
  if (pipe == NULL) {
    puts("Unable to open process");
    return -1;
  }
  char buffer[50];
  char *buff_ret = fgets(buffer, sizeof(buffer), pipe);
  if (buff_ret == NULL) {
    pclose(pipe);
    return -1;
  }
  if (sscanf(buffer, "%i", &result) != 1) {
    pclose(pipe);
    return -1;
  }
  int status = pclose(pipe);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return -1;
  }
  return result;
}

int i2c_write(uint8_t adr_slave, uint8_t adr_register, uint8_t data) {
  char *string;
  if (0 > asprintf(&string, "i2cset -y 1 0x%02x 0x%02x 0x%02x", adr_slave, adr_register, data))
    return -1;
  FILE *pipe = popen(string, "r");
  free(string);
  if (pipe == NULL) {
    puts("Unable to open process");
    return -1;
  }
  int status = pclose(pipe);
  return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int i2c_read_burst(uint8_t adr_slave, uint8_t start_reg, uint8_t *data,
                          int len) {
  char *string;
  if (0 > asprintf(&string, "i2ctransfer -y 1 w1@0x%02x 0x%02x r%d",
                   adr_slave, start_reg, len))
    return -1;
  FILE *pipe = popen(string, "r");
  free(string);
  if (pipe == NULL)
    return -1;

  char buffer[256];
  if (fgets(buffer, sizeof(buffer), pipe) == NULL) {
    pclose(pipe);
    return -1;
  }

  int status = pclose(pipe);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return -1;

  char *ptr = buffer;
  for (int i = 0; i < len; i++) {
    char *endptr;
    long val = strtol(ptr, &endptr, 0);
    if (endptr == ptr)
      return -1;
    data[i] = (uint8_t)val;
    ptr = endptr;
  }
  return 0;
}

void getRawAcc(int16_t *xx, int16_t *yy, int16_t *zz) {
  uint8_t data[6];
  if (i2c_read_burst(MPU6050_ADDR, 0x3B, data, 6) != 0) {
    *xx = *yy = *zz = 0;
    return;
  }
  *xx = (int16_t)((data[0] << 8) | data[1]);
  *yy = (int16_t)((data[2] << 8) | data[3]);
  *zz = (int16_t)((data[4] << 8) | data[5]);
}

void getRawGyro(int16_t *roll, int16_t *pitch, int16_t *yaw) {
  uint8_t data[6];
  if (i2c_read_burst(MPU6050_ADDR, 0x43, data, 6) != 0) {
    *roll = *pitch = *yaw = 0;
    return;
  }
  *roll = (int16_t)((data[0] << 8) | data[1]);
  *pitch = (int16_t)((data[2] << 8) | data[3]);
  *yaw = (int16_t)((data[4] << 8) | data[5]);
}

void imuConfig(void) {
  i2c_write(MPU6050_ADDR, 0x6B, 0x00); // wake up (clear sleep bit)

  // DLPF = 3 (acc BW 44Hz, gyro BW 42Hz) for boat use
  uint8_t dlpf = 3;
  uint8_t gyro_out_rate = 1; // 1kHz when DLPF 1-6

  // Sample rate = gyro_out_rate / (1 + SMPLRT_DIV)
  // Target ~100Hz → divider = 9
  uint8_t samplediv = (gyro_out_rate * 1000 / 100) - 1;
  i2c_write(MPU6050_ADDR, 0x19, samplediv);

  uint8_t config_val = dlpf; // no ext sync
  i2c_write(MPU6050_ADDR, 0x1A, config_val);
  i2c_write(MPU6050_ADDR, 0x1B, GYRO_RANGE << 3);
  i2c_write(MPU6050_ADDR, 0x1C, ACCEL_RANGE << 3);
}

void getGyro(float *roll, float *pitch, float *yaw) {
  int16_t x, y, z;
  getRawGyro(&x, &y, &z);
  float scale = GYRO_SCALE_FACTOR[GYRO_RANGE] / 32768.0f;
  *roll = (float)x * scale;
  *pitch = (float)y * scale;
  *yaw = (float)z * scale;
}

void getAccel(float *x, float *y, float *z) {
  int16_t rx, ry, rz;
  getRawAcc(&rx, &ry, &rz);
  float scale = ACCEL_SCALE_FACTOR[ACCEL_RANGE] / 32768.0f;
  *x = (float)rx * scale;
  *y = (float)ry * scale;
  *z = (float)rz * scale;
}
