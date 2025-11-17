// #include "gps.h"
// #include "lib/imu/mpu6050.h"
#include "lora.h"
#include "mpu6050.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define GPS_BAUDRATE 115200
#define GPS_TIMEOUT 1000

size_t format_packet(char *message, int temp1, int temp2, int temp3,
                     float accel, double lat, double lon, float bat, float watt,
                     int tilt, int head, int curr);

size_t message_len = 0;
int temp1 = 0;
int temp2 = 0;
int temp3 = 0;
float accel = 0.0;
double lat = 0.0;
double lon = 0.0;
float bat = 0.0;
float watt = 0.0;
int tilt = 0;
int head = 0;
int curr = 0;

// IMU stuff
int accel_x = 0;
int accel_y = 0;
int accel_z = 0;

int gyro_roll = 0;
int gyro_pitch = 0;
int gyro_yaw = 0;

int main(void) {
  lora_t *lora = lora_init("/dev/spidev0.0", 1000000);
  if (lora == NULL) {
    return 1;
  }

  // gps_t *gps = gps_init();
  // if (gps == NULL) {
  //   return 1;
  // }

  char message[2048];

  imuConfig();

  // lora_set_frequency(lora, 433E6);
  // lora_set_bandwith(lora, 125E3);
  // lora_set_spreading_factor(lora, 12);
  // lora_set_coding_rate(lora, 8);
  // lora_enable_crc(lora, true);
  // lora_set_tx_power(lora, 17);
  printf("hello\n");

  while (1) {
    // gps_location(gps);
    // lat = gps->loc.lat;
    // lon = gps->loc.lon;
    // head = gps->loc.course;

    getRawAcc(&accel_x, &accel_y, &accel_z);
    getRawGyro(&gyro_roll, &gyro_pitch, &gyro_yaw);

    accel = sqrt(pow(accel_x, 2) + pow(accel_y, 2) + pow(accel_z, 2));
    tilt = gyro_pitch;

    message_len = format_packet(message, temp1, temp2, temp3, accel, lat, lon,
                                bat, watt, tilt, head, curr);
    lora_send(lora, message, message_len, 1000);

    // pinnt accel variables
    printf("accel: %f ", accel);
    printf("tilt: %d ", tilt);
    printf("raw accel: %d %d %d ", accel_x, accel_y, accel_z);
    printf("raw gyro: %d %d %d\n", gyro_roll, gyro_pitch, gyro_yaw);
  }

  return 0;
}

size_t format_packet(char *message, int temp1, int temp2, int temp3,
                     float accel, double lat, double lon, float bat, float watt,
                     int tilt, int head, int curr) {
  return asprintf(&message,
                  "t1 %d,t2 %d,t3 %d,acc %f,lat %f,lon %f,bat %f,wat %f,tilt "
                  "%d,head %d,cur %d",
                  temp1, temp2, temp3, accel, lat, lon, bat, watt, tilt, head,
                  curr);
}
