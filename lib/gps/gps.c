#include "gps.h"
#include "../uart/uart.h"
#include "nmea.h"

#include <math.h>
#include <stdlib.h>

#ifndef GPS_DEVICE
#define GPS_DEVICE "/dev/serial0"
#endif

#define GPS_BAUDRATE (uint16_t)9600

gps_t *gps_init(void) {
  gps_t *gps = malloc(sizeof(gps_t));
  gps->uart = uart_init(GPS_DEVICE, GPS_BAUDRATE);

  return gps;
}

void gps_location(gps_t *gps) {
  uint8_t status = NMEA_STATUS_EMPTY;
  while (status != NMEA_STATUS_COMPLETED) {
    gpgga_t gpgga;
    gprmc_t gprmc;

    char buf[256];
    uart_read(gps->uart, buf, 256);

    switch (nmea_get_message_type(buf)) {
    case NMEA_GPGGA:
      nmea_parse_gpgga(buf, &gpgga);

      gps_convert_deg_to_dec(&gpgga.latitude, gpgga.lat, &gpgga.longitude,
                             gpgga.lon);

      gps->loc.lat = gpgga.latitude;
      gps->loc.lon = gpgga.longitude;
      gps->loc.alt = gpgga.alttitude;

      status |= NMEA_GPGGA;
      break;
    case NMEA_GPRMC:
      nmea_parse_gprmc(buf, &gprmc);

      gps->loc.speed = gprmc.speed;
      gps->loc.course = gprmc.course;

      status |= NMEA_GPRMC;
      break;
    }
  }
}

double gps_deg_to_dec(double deg) {
  double ddeg, sec = modf(deg, &ddeg) * 60;
  int _deg = (int)(ddeg / 100);
  int min = (int)(deg - (_deg * 100));

  double absdlat = round(deg * 1000000.);
  double absmlat = round(min * 1000000.);
  double absslat = round(sec * 1000000.);

  return round(absdlat + (absmlat / 60) + (absslat / 3600)) / 1000000;
}

void gps_convert_deg_to_dec(double *lat, char ns, double *lon, char we) {
  double _lat = (ns == 'N') ? *lat : -1 * (*lat);
  double _lon = (we == 'E') ? *lon : -1 * (*lon);

  *lat = gps_deg_to_dec(_lat);
  *lon = gps_deg_to_dec(_lon);
}

void gps_close(gps_t *gps) {
  uart_close(gps->uart);
  free(gps);
}
