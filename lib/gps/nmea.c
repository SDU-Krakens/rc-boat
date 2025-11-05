#include "nmea.h"
#include <stdlib.h>
#include <string.h>

uint8_t nmea_get_message_type(const char *message) {
  uint8_t checksum = 0;
  if ((checksum = nmea_valid_checksum(message)) != NMEA_STATUS_EMPTY) {
    return checksum;
  }

  if (strstr(message, NMEA_GPGGA_STR) != NULL) {
    return NMEA_GPGGA;
  }
  if (strstr(message, NMEA_GPRMC_STR) != NULL) {
    return NMEA_GPRMC;
  }

  return NMEA_UNKNOWN;
}

uint8_t nmea_valid_checksum(const char *message) {
  uint8_t checksum = (uint8_t)strtol(strchr(message, '*'), NULL, 16);

  char p;
  uint8_t sum = 0;
  ++message;
  while ((p = *message++) != '*') {
    sum ^= p;
  }

  if (sum != checksum) {
    return NMEA_CHECKSUM_ERR;
  }

  return NMEA_STATUS_EMPTY;
}

void nmea_parse_gpgga(const char *message, gpgga_t *gpgga);
void nmea_parse_gprmc(const char *message, gprmc_t *gprmc);
