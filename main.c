#include "gps.h"
#include "lora.h"
#include "mpu6050.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOOP_DELAY_US 100000 // 100ms padding between TX cycles
#define LORA_TX_TIMEOUT 500  // 500ms max wait for TX done
#define ALPHA 0.98f          // complementary filter coefficient
#define DEG_TO_RAD (M_PI / 180.0f)
#define RAD_TO_DEG (180.0f / M_PI)

size_t format_packet(char **message, int temp1, int temp2, int temp3, int volt,
                     float accel, double lat, double lon, float bat, float watt,
                     float roll, float pitch, float yaw, int head);

size_t message_len = 0;
int temp1 = 0;
int temp2 = 0;
int temp3 = 0;
float accel = 0.0;
double lat = 0.0;
double lon = 0.0;
float bat = 0.0;
float watt = 0.0;
float roll_angle = 0.0f;
float pitch_angle = 0.0f;
float yaw_angle = 0.0f;
int head = 0;
int curr = 0;
int volt = 0;

// IMU stuff
int16_t accel_x = 0;
int16_t accel_y = 0;
int16_t accel_z = 0;

int16_t gyro_roll = 0;
int16_t gyro_pitch = 0;
int16_t gyro_yaw = 0;

int main(void) {
  lora_t *lora = lora_init("/dev/spidev0.0", 1000000, 22);
  if (lora == NULL) {
    printf("Failed to init LoRa\n");
    return 1;
  }

  char *message = NULL; // Change to pointer

  // Initialize GPS
  gps_t *gps = gps_init();
  if (gps == NULL) {
    printf("Failed to init GPS\n");
    lora_end(lora);
    return 1;
  }

  // Initialize IMU
  imuConfig();
  lora_set_frequency(lora, 433E6);
  lora_set_bandwith(lora, 125E3);
  lora_set_spreading_factor(lora, 7);
  lora_set_coding_rate(lora, 8);
  lora_enable_crc(lora, true);
  lora_set_tx_power(lora, 17);
  printf("LoRa initialized\n");

  while (1) {
    // Read GPS data
    printf("Reading GPS\n");
    gps_location(gps);
    lat = gps->loc.lat;
    lon = gps->loc.lon;
    head = (int)gps->loc.course;

    // Read IMU data
    printf("Reading IMU\n");
    getRawAcc(&accel_x, &accel_y, &accel_z);
    getRawGyro(&gyro_roll, &gyro_pitch, &gyro_yaw);

    // Convert raw to physical units
    float ascale = ACCEL_SCALE_FACTOR[ACCEL_RANGE] / 32768.0f;
    float ax = accel_x * ascale;
    float ay = accel_y * ascale;
    float az = accel_z * ascale;
    accel = sqrtf(ax * ax + ay * ay + az * az);

    float gscale = GYRO_SCALE_FACTOR[GYRO_RANGE] / 32768.0f;
    float gx = gyro_roll * gscale;
    float gy = gyro_pitch * gscale;
    float gz = gyro_yaw * gscale;

    // dt in seconds (loop delay + processing)
    float dt = LOOP_DELAY_US / 1000000.0f;

    // Accel-based angles
    float accel_roll = atan2f(ay, az) * RAD_TO_DEG;
    float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;

    // Complementary filter
    roll_angle = ALPHA * (roll_angle + gx * dt) + (1.0f - ALPHA) * accel_roll;
    pitch_angle = ALPHA * (pitch_angle + gy * dt) + (1.0f - ALPHA) * accel_pitch;
    yaw_angle += gz * dt; // no accel correction for yaw (no magnetometer)

    if (message) {
      free(message); // Free previous allocation
      message = NULL;
    }

    message_len = format_packet(&message, temp1, temp2, temp3, volt, accel, lat,
                                lon, bat, watt, roll_angle, pitch_angle, yaw_angle, head);

    if (message_len > 0 && message) {
      bool sent = lora_send(lora, message, message_len, LORA_TX_TIMEOUT);
      printf("Sent %db OK?: %s\n", (int)message_len, sent ? "OK" : "FAIL");
      printf("Message: %s\n", message);
    } else {
      printf("Failed to format packet\n");
    }

    usleep(LOOP_DELAY_US);
  }

  if (message)
    free(message);

  lora_end(lora);
  gps_close(gps);
  return 0;
}

size_t format_packet(char **message, int temp1, int temp2, int temp3, int volt,
                     float accel, double lat, double lon, float bat, float watt,
                     float roll, float pitch, float yaw, int head) {
  int result =
      asprintf(message,
               "t1 %d,t2 %d,t3 %d,voltage %d,lat %f,lon %f,acceleration %f,"
               "current %d,water %d,roll %f,pitch %f,yaw %f,heading %d",
               temp1, temp2, temp3, volt, lat, lon, accel, (int)bat,
               (int)watt, roll, pitch, yaw, head);
  return (result >= 0) ? (size_t)result : 0;
}
