#include <gps.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  // Main loop
  gps_data_t *gps_data = malloc(sizeof(gps_data_t));
  for (;;) {
    // do stuff
    gps_get_data(gps_data);

    printf("Latitude: %d\n", gps_data->lat);
    printf("Longitude: %d\n", gps_data->lon);
  }
}
