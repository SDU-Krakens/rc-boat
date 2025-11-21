#include "gps.h"
#include "lora.h"
#include "mpu6050.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define GPS_BAUDRATE 115200
#define GPS_TIMEOUT 1000

size_t format_packet(char **message, int temp1, int temp2, int temp3, int volt,
		     float accel, double lat, double lon, float bat, float watt,
		     int tilt, int head);

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
int volt = 0;

// IMU stuff
int accel_x = 0;
int accel_y = 0;
int accel_z = 0;

int gyro_roll = 0;
int gyro_pitch = 0;
int gyro_yaw = 0;

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
	imu_config();

	lora_set_frequency(lora, 433E6);
	lora_set_bandwith(lora, 125E3);
	lora_set_spreading_factor(lora, 7);
	lora_set_coding_rate(lora, 8);
	lora_enable_crc(lora, true);
	lora_set_tx_power(lora, 17);
	printf("LoRa initialized\n");

	while (1) {
		// Read GPS data
		gps_location(gps);
		lat = gps->loc.lat;
		lon = gps->loc.lon;

		// Read IMU data
		get_raw_acc(&accel_x, &accel_y, &accel_z);
		get_raw_gyro(&gyro_roll, &gyro_pitch, &gyro_yaw);

		accel =
		    sqrt(pow(accel_x, 2) + pow(accel_y, 2) + pow(accel_z, 2));
		tilt = gyro_pitch;

		if (message) {
			free(message); // Free previous allocation
			message = NULL;
		}

		message_len =
		    format_packet(&message, temp1, temp2, temp3, volt, accel,
				  lat, lon, bat, watt, tilt, head);

		if (message_len > 0 && message) {
			bool sent =
			    lora_send(lora, message, message_len, 15000);
			printf("Sent %db OK?: %s\n", (int)message_len,
			       sent ? "OK" : "FAIL");
			printf("Message: %s\n", message);
		} else {
			printf("Failed to format packet\n");
		}

		// Add delay to avoid CPU overuse
		usleep(500000);
	}

	if (message)
		free(message);
	// TODO: Add GPS cleanup function when implemented
	lora_end(lora);
	return 0;
}

size_t format_packet(char **message, int temp1, int temp2, int temp3, int volt,
		     float accel, double lat, double lon, float bat, float watt,
		     int tilt, int head) {
	int result = asprintf(
	    message,
	    "t1 %d,t2 %d,t3 %d, voltage %d, lat %f,lon %f, acceleration %d, "
	    "current %d,water %d,tilt "
	    "%d,heading %d",
	    temp1, temp2, temp3, volt, lat, lon, (int)accel, (int)bat,
	    (int)watt, tilt, head);
	return (result >= 0) ? (size_t)result : 0;
