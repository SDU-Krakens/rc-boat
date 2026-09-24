// Basic build test: opens every driver and prints readings
#include "can.h"
#include "config.h"
#include "gps.h"
#include "i2c.h"
#include "imu.h"
#include "madgwick.h"
#include "storage.h"
#include "uart.h"

#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <stdio.h>

int main(void) {
  stdio_init_all();

  i2c_t *bus0 = i2c_open(i2c_get_instance(CONF_IMU0_I2C), CONF_IMU0_SDA,
                         CONF_IMU0_SCL, CONF_I2C_BAUD);
  i2c_t *bus1 = i2c_open(i2c_get_instance(CONF_IMU1_I2C), CONF_IMU1_SDA,
                         CONF_IMU1_SCL, CONF_I2C_BAUD);
  storage_t *st = storage_open();

  imu_t *bno = imu_open(IMU_BNO055, bus0, &IMU_ROT_IDENTITY);
  imu_t *icm = imu_open(IMU_ICM20948, bus1, &IMU_ROT_IDENTITY);
  if (bno) {
    imu_cal_load(bno, st);
  }
  if (icm && imu_cal_load(icm, st) != 0) {
    imu_cal_gyro(icm, CONF_GYRO_CAL_SAMPLES);
  }

  uart_t *gps_uart = uart_open(uart_get_instance(CONF_GPS_UART),
                               CONF_GPS_TX_PIN, CONF_GPS_RX_PIN, CONF_GPS_BAUD);
  gps_t *gps = gps_open(gps_uart);

  can_t *can = can_open(CONF_CAN_PIO, CONF_CAN_RX_PIN, CONF_CAN_TX_PIN,
                        CONF_CAN_BITRATE);

  printf("bno %s, icm %s, gps %s, can %s\n", bno ? "ok" : "FAIL",
         icm ? "ok" : "FAIL", gps ? "ok" : "FAIL", can ? "ok" : "FAIL");

  madgwick_t combined;
  madgwick_open(&combined, CONF_MADGWICK_BETA);

  imu_sample_t s0 = {0}, s1 = {0}, avg = {0};
  uint32_t period_us = 1000000u / CONF_IMU_SAMPLE_HZ;

  while (true) {
    if (bno) {
      imu_read(bno, &s0);
    }
    if (icm) {
      imu_read(icm, &s1);
    }
    if (bno && icm) {
      imu_average(&s0, &s1, &avg);
      if (avg.valid & IMU_VALID_MAG) {
        madgwick_update(&combined, avg.gyro, avg.accel, avg.mag,
                        period_us / 1e6f);
        madgwick_euler(&combined, &avg.roll, &avg.pitch, &avg.yaw);
      }
    }

    if (gps && gps_poll(gps)) {
      printf("gps fix %u sv %u lat %ld lon %ld\n", gps->fix.fix_type,
             gps->fix.num_sv, (long)gps->fix.lat, (long)gps->fix.lon);
    }

    can_frame_t frame;
    while (can && can_receive(can, &frame)) {
      printf("can %lx dlc %u\n", (unsigned long)frame.id, frame.dlc);
    }

    printf("bno r %.2f p %.2f y %.2f | icm r %.2f p %.2f y %.2f | "
           "avg r %.2f p %.2f y %.2f\n",
           s0.roll, s0.pitch, s0.yaw, s1.roll, s1.pitch, s1.yaw, avg.roll,
           avg.pitch, avg.yaw);

    sleep_us(period_us);
  }
}
