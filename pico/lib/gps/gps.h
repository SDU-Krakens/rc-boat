#pragma once

#include "../uart/uart.h"
#include "ubx.h"
#include <stdbool.h>
#include <stdint.h>

// NEO-M8M factory default baud rate
#define GPS_DEFAULT_BAUD 9600

// Module-side port id of its UART in CFG-PRT
#define GPS_PORT_UART1 1

// CFG-PRT payload fields
#define GPS_CFG_PRT_LEN 20
#define GPS_PRT_MODE_8N1 0x000008D0u
#define GPS_PROTO_UBX 0x0001
// CFG-RATE payload
#define GPS_CFG_RATE_LEN 6
#define GPS_TIME_REF_GPS 1
#define GPS_NAV_RATE 1
// CFG-MSG payload (message class, id, rate on current port)
#define GPS_CFG_MSG_LEN 3
#define GPS_MSG_RATE 1

// NAV-PVT payload offsets
#define GPS_NAV_PVT_LEN 92
#define GPS_PVT_YEAR 4
#define GPS_PVT_MONTH 6
#define GPS_PVT_DAY 7
#define GPS_PVT_HOUR 8
#define GPS_PVT_MIN 9
#define GPS_PVT_SEC 10
#define GPS_PVT_VALID 11
#define GPS_PVT_FIX_TYPE 20
#define GPS_PVT_NUM_SV 23
#define GPS_PVT_LON 24
#define GPS_PVT_LAT 28
#define GPS_PVT_HEIGHT 32
#define GPS_PVT_GSPEED 60
#define GPS_PVT_HEAD_MOT 64

// Time to let the module switch baud rate after CFG-PRT
#define GPS_BAUD_SWITCH_MS 100

typedef struct {
  int32_t lat, lon;    // 1e-7 deg
  int32_t height_mm;   // above ellipsoid
  int32_t gspeed_mm_s;
  int32_t heading_1e5; // 1e-5 deg
  uint8_t fix_type, num_sv;
  uint16_t year;
  uint8_t month, day, hour, min, sec;
  uint8_t valid; // NAV-PVT valid flags
} gps_fix_t;

typedef struct {
  uart_t *uart;
  ubx_t ubx;
  gps_fix_t fix;
} gps_t;

gps_t *gps_open(uart_t *uart);
void gps_close(gps_t *gps);
bool gps_poll(gps_t *gps);
