#include "gps.h"
#include "config.h"
#include "log.h"

#include "hardware/uart.h"
#include "pico/time.h"
#include <stdlib.h>

#define MS_PER_S 1000
#define GPS_READ_CHUNK 64

static void put_u16(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}

static void put_u32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) {
    p[i] = (v >> (8 * i)) & 0xFF;
  }
}

static uint16_t get_u16(const uint8_t *p) { return p[0] | (p[1] << 8); }

static int32_t get_i32(const uint8_t *p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void send(gps_t *gps, uint8_t cls, uint8_t id, const uint8_t *payload,
                 uint16_t len) {
  uint8_t frame[UBX_FRAME_LEN(UBX_MAX_PAYLOAD)];
  size_t n = ubx_build(frame, cls, id, payload, len);
  uart_send(gps->uart, frame, n);
}

// Wait for ACK-ACK (0) or ACK-NAK / timeout (-1) for message cls/id
static int wait_ack(gps_t *gps, uint8_t cls, uint8_t id) {
  absolute_time_t deadline = make_timeout_time_ms(CONF_GPS_ACK_TIMEOUT_MS);
  uint8_t byte;

  while (!time_reached(deadline)) {
    if (uart_read(gps->uart, &byte, 1) != 1) {
      continue;
    }
    if (!ubx_parse(&gps->ubx, byte) || gps->ubx.cls != UBX_CLASS_ACK ||
        gps->ubx.len != 2 || gps->ubx.payload[0] != cls ||
        gps->ubx.payload[1] != id) {
      continue;
    }
    return gps->ubx.id == UBX_ACK_ACK ? 0 : -1;
  }
  return -1;
}

static void send_cfg_prt(gps_t *gps) {
  uint8_t p[GPS_CFG_PRT_LEN] = {0};
  p[0] = GPS_PORT_UART1;
  put_u32(&p[4], GPS_PRT_MODE_8N1);
  put_u32(&p[8], CONF_GPS_BAUD);
  put_u16(&p[12], GPS_PROTO_UBX); // in: UBX only
  put_u16(&p[14], GPS_PROTO_UBX); // out: UBX only, NMEA off
  send(gps, UBX_CLASS_CFG, UBX_CFG_PRT, p, sizeof(p));
}

static int send_rate(gps_t *gps, uint32_t hz) {
  uint8_t rate[GPS_CFG_RATE_LEN];
  put_u16(&rate[0], MS_PER_S / hz);
  put_u16(&rate[2], GPS_NAV_RATE);
  put_u16(&rate[4], GPS_TIME_REF_GPS);
  send(gps, UBX_CLASS_CFG, UBX_CFG_RATE, rate, sizeof(rate));
  return wait_ack(gps, UBX_CLASS_CFG, UBX_CFG_RATE);
}

static int configure(gps_t *gps) {
  // Module may still be at its default baud: switch it to CONF_GPS_BAUD
  uart_set_baud(gps->uart, GPS_DEFAULT_BAUD);
  send_cfg_prt(gps);
  uart_tx_wait_blocking(gps->uart->inst);
  sleep_ms(GPS_BAUD_SWITCH_MS);

  // Repeat at the new baud; the ACK proves the module talks at that rate
  uart_set_baud(gps->uart, CONF_GPS_BAUD);
  ubx_open(&gps->ubx);
  send_cfg_prt(gps);
  if (wait_ack(gps, UBX_CLASS_CFG, UBX_CFG_PRT) != 0) {
    return -1;
  }

  if (send_rate(gps, CONF_GPS_RATE_HZ) != 0) {
    return -1;
  }

  uint8_t msg[GPS_CFG_MSG_LEN] = {UBX_CLASS_NAV, UBX_NAV_PVT, GPS_MSG_RATE};
  send(gps, UBX_CLASS_CFG, UBX_CFG_MSG, msg, sizeof(msg));
  return wait_ack(gps, UBX_CLASS_CFG, UBX_CFG_MSG);
}

gps_t *gps_open(uart_t *uart) {
  gps_t *gps = calloc(1, sizeof(gps_t));
  if (!gps) {
    return NULL;
  }
  gps->uart = uart;
  ubx_open(&gps->ubx);

  for (int i = 0; i < CONF_GPS_CFG_RETRIES; i++) {
    if (configure(gps) == 0) {
      log_info(LOG_SRC_GPS, "configured after %d attempt(s)", i + 1);
      return gps;
    }
  }

  log_err(LOG_SRC_GPS, "no ACK from module after %d attempts",
          CONF_GPS_CFG_RETRIES);
  free(gps);
  return NULL;
}

void gps_close(gps_t *gps) { free(gps); }

int gps_set_rate(gps_t *gps, uint32_t hz) {
  if (hz == 0 || send_rate(gps, hz) != 0) {
    log_err(LOG_SRC_GPS, "can't set rate %lu Hz", (unsigned long)hz);
    return -1;
  }
  log_info(LOG_SRC_GPS, "rate %lu Hz", (unsigned long)hz);
  return 0;
}

static void decode_pvt(gps_fix_t *fix, const uint8_t *p) {
  fix->year = get_u16(&p[GPS_PVT_YEAR]);
  fix->month = p[GPS_PVT_MONTH];
  fix->day = p[GPS_PVT_DAY];
  fix->hour = p[GPS_PVT_HOUR];
  fix->min = p[GPS_PVT_MIN];
  fix->sec = p[GPS_PVT_SEC];
  fix->valid = p[GPS_PVT_VALID];
  fix->fix_type = p[GPS_PVT_FIX_TYPE];
  fix->num_sv = p[GPS_PVT_NUM_SV];
  fix->lon = get_i32(&p[GPS_PVT_LON]);
  fix->lat = get_i32(&p[GPS_PVT_LAT]);
  fix->height_mm = get_i32(&p[GPS_PVT_HEIGHT]);
  fix->gspeed_mm_s = get_i32(&p[GPS_PVT_GSPEED]);
  fix->heading_1e5 = get_i32(&p[GPS_PVT_HEAD_MOT]);
}

bool gps_poll(gps_t *gps) {
  uint8_t buf[GPS_READ_CHUNK];
  bool updated = false;
  size_t n;

  while ((n = uart_read(gps->uart, buf, sizeof(buf))) > 0) {
    for (size_t i = 0; i < n; i++) {
      if (ubx_parse(&gps->ubx, buf[i]) && gps->ubx.cls == UBX_CLASS_NAV &&
          gps->ubx.id == UBX_NAV_PVT && gps->ubx.len == GPS_NAV_PVT_LEN) {
        decode_pvt(&gps->fix, gps->ubx.payload);
        updated = true;
      }
    }
  }
  return updated;
}
