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
  char *checksum_ptr = strchr(message, '*');
  if (checksum_ptr == NULL) {
    return NMEA_CHECKSUM_ERR;
  }

  uint8_t checksum = (uint8_t)strtol(checksum_ptr + 1, NULL, 16);

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

// Helper function to extract the next field from NMEA message
// static char *nmea_get_field(char **str, int field_num) {
//  char *start = *str;
//  int current_field = 0;
//
//  while (*start && current_field < field_num) {
//    if (*start == ',') {
//      current_field++;
//    }
//    start++;
//  }
//
//  if (current_field != field_num) {
//    return NULL;
//  }
//
//  char *end = start;
//  while (*end && *end != ',' && *end != '*') {
//    end++;
//  }
//
//  *str = end;
//  return start;
//}

// Parse GPGGA message
// $GPGGA,hhmmss.ss,llll.ll,N/S,yyyyy.yy,E/W,q,ss,x.x,alt,M,geoid,M,,*hh
// Fields: 0=GPGGA, 1=time, 2=lat, 3=N/S, 4=lon, 5=E/W, 6=quality, 7=sats,
// 8=hdop, 9=alt, 10=M
void nmea_parse_gpgga(const char *message, gpgga_t *gpgga) {
  if (gpgga == NULL || message == NULL) {
    return;
  }

  // Create a working copy since strtok modifies the string
  char buf[256];
  strncpy(buf, message, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *token;
  int field = 0;

  token = strtok(buf, ",");
  while (token != NULL) {
    switch (field) {
    case 2: // Latitude
      if (strlen(token) > 0) {
        gpgga->latitude = atof(token);
      }
      break;
    case 3: // N/S
      if (strlen(token) > 0) {
        gpgga->lat = token[0];
      }
      break;
    case 4: // Longitude
      if (strlen(token) > 0) {
        gpgga->longitude = atof(token);
      }
      break;
    case 5: // E/W
      if (strlen(token) > 0) {
        gpgga->lon = token[0];
      }
      break;
    case 6: // Fix quality
      if (strlen(token) > 0) {
        gpgga->quality = (uint8_t)atoi(token);
      }
      break;
    case 7: // Number of satellites
      if (strlen(token) > 0) {
        gpgga->satellites = (uint8_t)atoi(token);
      }
      break;
    case 9: // Altitude
      if (strlen(token) > 0) {
        gpgga->alttitude = atof(token);
      }
      break;
    }
    token = strtok(NULL, ",*");
    field++;
  }
}

// Parse GPRMC message
// $GPRMC,hhmmss.ss,A,llll.ll,N/S,yyyyy.yy,E/W,speed,course,ddmmyy,,,*hh
// Fields: 0=GPRMC, 1=time, 2=status, 3=lat, 4=N/S, 5=lon, 6=E/W, 7=speed,
// 8=course, 9=date
void nmea_parse_gprmc(const char *message, gprmc_t *gprmc) {
  if (gprmc == NULL || message == NULL) {
    return;
  }

  // Create a working copy
  char buf[256];
  strncpy(buf, message, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *token;
  int field = 0;

  token = strtok(buf, ",");
  while (token != NULL) {
    switch (field) {
    case 3: // Latitude
      if (strlen(token) > 0) {
        gprmc->latitude = atof(token);
      }
      break;
    case 4: // N/S
      if (strlen(token) > 0) {
        gprmc->lat = token[0];
      }
      break;
    case 5: // Longitude
      if (strlen(token) > 0) {
        gprmc->longitude = atof(token);
      }
      break;
    case 6: // E/W
      if (strlen(token) > 0) {
        gprmc->lon = token[0];
      }
      break;
    case 7: // Speed in knots
      if (strlen(token) > 0) {
        gprmc->speed = atof(token);
      }
      break;
    case 8: // Course over ground
      if (strlen(token) > 0) {
        gprmc->course = atof(token);
      }
      break;
    }
    token = strtok(NULL, ",*");
    field++;
  }
}
