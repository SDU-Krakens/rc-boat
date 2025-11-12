#include "gps.h"
#include "lora.h"
#include <stdio.h>
#include <stdlib.h>

size_t format_packet(char *message, int temp1, int temp2, int temp3,
                     float accel, double lat, double lon, float bat, float watt,
                     int tilt, int head, int curr);

char *message = NULL;
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

int main(void) {
  lora_t *lora = lora_init("/dev/spidev0.0", 1000000);
  if (lora == NULL) {
    return 1;
  }

  gps_t *gps = gps_init();
  if (gps == NULL) {
    return 1;
  }

  lora_set_frequency(lora, 433E6);
  lora_set_bandwith(lora, 125E3);
  lora_set_spreading_factor(lora, 12);
  lora_set_coding_rate(lora, 8);
  lora_enable_crc(lora, true);
  lora_set_tx_power(lora, 17);

  for (;;) {
    gps_location(gps);
    lat = gps->loc.lat;
    lon = gps->loc.lon;
    head = gps->loc.course;

    message_len = format_packet(message, temp1, temp2, temp3, accel, lat, lon,
                                bat, watt, tilt, head, curr);
    lora_send(lora, message, message_len, 1000);
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
