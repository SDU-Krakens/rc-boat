#pragma once

#include "../uart/uart.h"

typedef struct {
  double lat;
  double lon;
  double speed;
  double alt;
  double course;
} loc_t;

typedef struct {
  uart_t *uart;
  loc_t loc;
} gps_t;

gps_t *gps_init(void);
void gps_location(gps_t *gps);
double gps_deg_to_dec(double deg);
void gps_convert_deg_to_dec(double *lat, char ns, double *lon, char we);
void gps_close(gps_t *gps);
