#include "gps.h"
#include "../uart/uart.h"
#include "nmea.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef GPS_DEVICE
#define GPS_DEVICE "/dev/serial0"
#endif

#define GPS_BAUDRATE (uint16_t)9600

gps_t *gps_init(void) {
  gps_t *gps = malloc(sizeof(gps_t));
  if (!gps) {
    return NULL;
  }

  gps->uart = uart_init(GPS_DEVICE, GPS_BAUDRATE);
  if (!gps->uart) {
    free(gps);
    return NULL;
  }

  return gps;
}

void gps_location(gps_t *gps) {
  uint8_t status = NMEA_STATUS_EMPTY;
  while (status != NMEA_STATUS_COMPLETED) {
    gpgga_t gpgga;
    gprmc_t gprmc;

    char buf[256];
    int n = uart_read(gps->uart, buf, sizeof(buf) - 1);
    if (n <= 0) {
      continue;
    }
    buf[n] = '\0';

    switch (nmea_get_message_type(buf)) {
    case NMEA_GPGGA:
      nmea_parse_gpgga(buf, &gpgga);

      gps_convert_deg_to_dec(&gpgga.latitude, gpgga.lat, &gpgga.longitude,
                             gpgga.lon);

      gps->loc.lat = gpgga.latitude;
      gps->loc.lon = gpgga.longitude;
      gps->loc.alt = gpgga.altitude;

      status |= NMEA_GPGGA;
      break;
    case NMEA_GPRMC:
      nmea_parse_gprmc(buf, &gprmc);

      gps->loc.speed = gprmc.speed;
      gps->loc.course = gprmc.course;

      status |= NMEA_GPRMC;
      break;
    default:
      break;
    }
  }
}

double gps_deg_to_dec(double raw) {
  double degrees = (int)(raw / 100.0);
  double minutes = raw - degrees * 100.0;
  return degrees + minutes / 60.0;
}

void gps_convert_deg_to_dec(double *lat, char ns, double *lon, char we) {
  *lat = gps_deg_to_dec(*lat);
  *lon = gps_deg_to_dec(*lon);

  if (ns == 'S') {
    *lat = -*lat;
  }
  if (we == 'W') {
    *lon = -*lon;
  }
}

void gps_close(gps_t *gps) {
  uart_close(gps->uart);
  free(gps);
}
