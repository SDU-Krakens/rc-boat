#pragma once

#include <stdint.h>

#define NMEA_STATUS_EMPTY 0x00
#define NMEA_STATUS_COMPLETED 0x03

#define NMEA_GPRMC 0x01
#define NMEA_GPRMC_STR "$GPRMC"
#define NMEA_GPGGA 0x02
#define NMEA_GPGGA_STR "$GPGGA"
#define NMEA_UNKNOWN 0x00

#define NMEA_CHECKSUM_ERR 0x80
#define NMEA_MESSAGE_ERR 0xC0

typedef struct {
  double latitude;
  char lat;
  double longitude;
  char lon;
  uint8_t quality;
  uint8_t satellites;
  double alttitude;
} gpgga_t;

typedef struct {
  double latitude;
  char lat;
  double longitude;
  char lon;
  double speed;
  double course;
} gprmc_t;

uint8_t nmea_get_message_type(const char *message);
uint8_t nmea_valid_checksum(const char *message);

void nmea_parse_gpgga(const char *message, gpgga_t *gpgga);
void nmea_parse_gprmc(const char *message, gprmc_t *gprmc);
