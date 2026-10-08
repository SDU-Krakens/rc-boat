#include "comm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// CRC-16/CCITT-FALSE
#define CRC16_INIT 0xFFFF
#define CRC16_POLY 0x1021
#define CRC16_TOP_BIT 0x8000

// Offsets inside a frame
#define OFF_LEN 2
#define OFF_TYPE 4
#define OFF_COUNTER 5
#define OFF_DATA COMM_HEADER_LEN
#define CRC_START OFF_LEN // CRC covers len, type, counter, data

// Data lengths
#define IMU_LEN 84
#define GPS_LEN 30
#define CAN_HEAD_LEN 6 // id, flags, dlc
#define CAN_FLAG_EXT 0x01
#define CAN_FLAG_RTR 0x02
#define TEXT_HEAD_LEN 2 // severity, source
#define TEXT_ACK_LEN 1
#define REPLY_LEN 6 // what, id (4), result
#define COMMAND_LEN 1
#define COMMAND_SAMPLES_LEN 3
#define SETTING_LEN 5
#define FIXED_MAX IMU_LEN // largest fixed-size data

#define UNKNOWN_NAME "unknown"

// Writer / reader for little-endian values

static void put_u8(uint8_t **p, uint8_t v) { *(*p)++ = v; }

static void put_u16(uint8_t **p, uint16_t v) {
  put_u8(p, v & 0xFF);
  put_u8(p, v >> 8);
}

static void put_u32(uint8_t **p, uint32_t v) {
  put_u16(p, v & 0xFFFF);
  put_u16(p, v >> 16);
}

static void put_u64(uint8_t **p, uint64_t v) {
  put_u32(p, v & 0xFFFFFFFFu);
  put_u32(p, v >> 32);
}

static void put_i32(uint8_t **p, int32_t v) { put_u32(p, (uint32_t)v); }

static void put_f32(uint8_t **p, float v) {
  uint32_t bits;
  memcpy(&bits, &v, sizeof(bits));
  put_u32(p, bits);
}

typedef struct {
  const uint8_t *p;
  size_t left;
} reader_t;

static uint8_t get_u8(reader_t *r) {
  if (r->left == 0) {
    return 0;
  }
  r->left--;
  return *r->p++;
}

static uint16_t get_u16(reader_t *r) {
  uint16_t lo = get_u8(r);
  return lo | (uint16_t)get_u8(r) << 8;
}

static uint32_t get_u32(reader_t *r) {
  uint32_t lo = get_u16(r);
  return lo | (uint32_t)get_u16(r) << 16;
}

static uint64_t get_u64(reader_t *r) {
  uint64_t lo = get_u32(r);
  return lo | (uint64_t)get_u32(r) << 32;
}

static int32_t get_i32(reader_t *r) { return (int32_t)get_u32(r); }

static float get_f32(reader_t *r) {
  uint32_t bits = get_u32(r);
  float v;
  memcpy(&v, &bits, sizeof(v));
  return v;
}

static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = CRC16_INIT;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & CRC16_TOP_BIT) ? (crc << 1) ^ CRC16_POLY : crc << 1;
    }
  }
  return crc;
}

// Type byte -> slot in the per-type arrays, -1 if unknown
static int type_index(uint8_t type) {
  switch (type) {
  case COMM_IMU0: return 0;
  case COMM_IMU1: return 1;
  case COMM_IMU: return 2;
  case COMM_GPS: return 3;
  case COMM_CAN_RX: return 4;
  case COMM_TEXT: return 5;
  case COMM_REPLY: return 6;
  case COMM_COMMAND: return 7;
  case COMM_CAN_TX: return 8;
  case COMM_SETTING: return 9;
  case COMM_TEXT_ACK: return 10;
  default: return -1;
  }
}

comm_t *comm_open(comm_send_fn send, void *send_ctx) {
  comm_t *comm = calloc(1, sizeof(comm_t));
  if (!comm) {
    return NULL;
  }
  comm->send = send;
  comm->send_ctx = send_ctx;
  return comm;
}

void comm_close(comm_t *comm) { free(comm); }

void comm_on(comm_t *comm, comm_type_t type, comm_handler_fn fn, void *ctx) {
  int idx = type_index(type);
  if (idx < 0) {
    return;
  }
  comm->handler[idx] = fn;
  comm->handler_ctx[idx] = ctx;
}

void comm_on_error(comm_t *comm, comm_error_fn fn, void *ctx) {
  comm->on_error = fn;
  comm->error_ctx = ctx;
}

static void report(comm_t *comm, comm_error_t err, uint8_t type) {
  if (comm->on_error) {
    comm->on_error(comm->error_ctx, err, (comm_type_t)type);
  }
}

// Sending

static int send_frame(comm_t *comm, uint8_t type, uint8_t counter,
                      const uint8_t *data, size_t len) {
  if (len > COMM_DATA_MAX) {
    return -1;
  }
  size_t total = COMM_HEADER_LEN + len + COMM_CRC_LEN;
  uint8_t *frame = malloc(total);
  if (!frame) {
    return -1;
  }

  uint8_t *p = frame;
  put_u8(&p, COMM_MARKER_1);
  put_u8(&p, COMM_MARKER_2);
  put_u16(&p, (uint16_t)len);
  put_u8(&p, type);
  put_u8(&p, counter);
  memcpy(p, data, len);
  p += len;
  put_u16(&p, crc16(frame + CRC_START, total - CRC_START - COMM_CRC_LEN));

  comm->send(comm->send_ctx, frame, total);
  free(frame);
  return 0;
}

static void pack_imu(uint8_t **p, const imu_sample_t *s) {
  put_u64(p, s->timestamp_us);
  for (int i = 0; i < 3; i++) {
    put_f32(p, s->accel[i]);
  }
  for (int i = 0; i < 3; i++) {
    put_f32(p, s->gyro[i]);
  }
  for (int i = 0; i < 3; i++) {
    put_f32(p, s->mag[i]);
  }
  put_f32(p, s->temp);
  for (int i = 0; i < 4; i++) {
    put_f32(p, s->quat[i]);
  }
  put_f32(p, s->roll);
  put_f32(p, s->pitch);
  put_f32(p, s->yaw);
  put_u32(p, s->valid);
  put_u8(p, s->cal.sys);
  put_u8(p, s->cal.accel);
  put_u8(p, s->cal.gyro);
  put_u8(p, s->cal.mag);
}

static void pack_gps(uint8_t **p, const gps_fix_t *f) {
  put_i32(p, f->lat);
  put_i32(p, f->lon);
  put_i32(p, f->height_mm);
  put_i32(p, f->gspeed_mm_s);
  put_i32(p, f->heading_1e5);
  put_u8(p, f->fix_type);
  put_u8(p, f->num_sv);
  put_u16(p, f->year);
  put_u8(p, f->month);
  put_u8(p, f->day);
  put_u8(p, f->hour);
  put_u8(p, f->min);
  put_u8(p, f->sec);
  put_u8(p, f->valid);
}

static int pack_can(uint8_t **p, const can_frame_t *f) {
  if (f->dlc > CAN_MAX_DLC) {
    return -1;
  }
  put_u32(p, f->id);
  put_u8(p, (f->ext ? CAN_FLAG_EXT : 0) | (f->rtr ? CAN_FLAG_RTR : 0));
  put_u8(p, f->dlc);
  memcpy(*p, f->data, f->dlc);
  *p += f->dlc;
  return 0;
}

int comm_send(comm_t *comm, comm_type_t type, const void *msg) {
  int idx = type_index(type);
  if (idx < 0 || !msg) {
    return -1;
  }

  // Text has variable length, everything else fits in FIXED_MAX
  if (type == COMM_TEXT) {
    const comm_text_t *t = msg;
    size_t len = TEXT_HEAD_LEN + t->len;
    uint8_t *data = malloc(len);
    if (!data) {
      return -1;
    }
    data[0] = t->severity;
    data[1] = t->source;
    memcpy(data + TEXT_HEAD_LEN, t->text, t->len);
    uint8_t counter = comm->tx_counter[idx];
    int rc = send_frame(comm, type, counter, data, len);
    free(data);
    if (rc != 0) {
      return -1;
    }
    comm->tx_counter[idx]++;
    return counter;
  }

  uint8_t data[FIXED_MAX];
  uint8_t *p = data;

  switch (type) {
  case COMM_IMU0:
  case COMM_IMU1:
  case COMM_IMU: pack_imu(&p, msg); break;
  case COMM_GPS: pack_gps(&p, msg); break;
  case COMM_CAN_RX:
  case COMM_CAN_TX:
    if (pack_can(&p, msg) != 0) {
      return -1;
    }
    break;
  case COMM_REPLY: {
    const comm_reply_t *r = msg;
    put_u8(&p, r->what);
    put_u32(&p, r->id);
    put_u8(&p, r->result);
    break;
  }
  case COMM_COMMAND: {
    const comm_command_t *c = msg;
    put_u8(&p, c->id);
    if (c->id == COMM_CMD_GYRO_CAL && c->samples) {
      put_u16(&p, c->samples);
    }
    break;
  }
  case COMM_SETTING: {
    const comm_setting_t *s = msg;
    put_u8(&p, s->id);
    put_u32(&p, s->u32); // same bits for u32 and f32 values
    break;
  }
  case COMM_TEXT_ACK: put_u8(&p, *(const uint8_t *)msg); break;
  default: return -1;
  }

  uint8_t counter = comm->tx_counter[idx];
  if (send_frame(comm, type, counter, data, p - data) != 0) {
    return -1;
  }
  comm->tx_counter[idx]++;
  return counter;
}

int comm_rx_counter(comm_t *comm, comm_type_t type) {
  int idx = type_index(type);
  if (idx < 0) {
    return -1;
  }
  return comm->rx_counter[idx];
}

static int vsend_text(comm_t *comm, uint8_t counter, log_sev_t sev,
                      log_src_t src, const char *fmt, va_list ap) {
  va_list copy;
  va_copy(copy, ap);
  int n = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);
  if (n < 0 || (size_t)n + TEXT_HEAD_LEN > COMM_DATA_MAX) {
    return -1;
  }

  // +1 for the terminator vsnprintf writes, not sent
  uint8_t *data = malloc(TEXT_HEAD_LEN + n + 1);
  if (!data) {
    return -1;
  }
  data[0] = sev;
  data[1] = src;
  vsnprintf((char *)data + TEXT_HEAD_LEN, n + 1, fmt, ap);

  int rc = send_frame(comm, COMM_TEXT, counter, data, TEXT_HEAD_LEN + n);
  free(data);
  return rc;
}

int comm_send_text(comm_t *comm, log_sev_t sev, log_src_t src, const char *fmt,
                   ...) {
  int idx = type_index(COMM_TEXT);
  uint8_t counter = comm->tx_counter[idx];

  va_list ap;
  va_start(ap, fmt);
  int rc = vsend_text(comm, counter, sev, src, fmt, ap);
  va_end(ap);
  if (rc != 0) {
    return -1;
  }
  comm->tx_counter[idx]++;
  return counter;
}

int comm_resend_text(comm_t *comm, uint8_t counter, log_sev_t sev,
                     log_src_t src, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int rc = vsend_text(comm, counter, sev, src, fmt, ap);
  va_end(ap);
  return rc;
}

// Receiving

static bool unpack_imu(reader_t *r, imu_sample_t *s) {
  if (r->left != IMU_LEN) {
    return false;
  }
  s->timestamp_us = get_u64(r);
  for (int i = 0; i < 3; i++) {
    s->accel[i] = get_f32(r);
  }
  for (int i = 0; i < 3; i++) {
    s->gyro[i] = get_f32(r);
  }
  for (int i = 0; i < 3; i++) {
    s->mag[i] = get_f32(r);
  }
  s->temp = get_f32(r);
  for (int i = 0; i < 4; i++) {
    s->quat[i] = get_f32(r);
  }
  s->roll = get_f32(r);
  s->pitch = get_f32(r);
  s->yaw = get_f32(r);
  s->valid = get_u32(r);
  s->cal.sys = get_u8(r);
  s->cal.accel = get_u8(r);
  s->cal.gyro = get_u8(r);
  s->cal.mag = get_u8(r);
  return true;
}

static bool unpack_gps(reader_t *r, gps_fix_t *f) {
  if (r->left != GPS_LEN) {
    return false;
  }
  f->lat = get_i32(r);
  f->lon = get_i32(r);
  f->height_mm = get_i32(r);
  f->gspeed_mm_s = get_i32(r);
  f->heading_1e5 = get_i32(r);
  f->fix_type = get_u8(r);
  f->num_sv = get_u8(r);
  f->year = get_u16(r);
  f->month = get_u8(r);
  f->day = get_u8(r);
  f->hour = get_u8(r);
  f->min = get_u8(r);
  f->sec = get_u8(r);
  f->valid = get_u8(r);
  return true;
}

static bool unpack_can(reader_t *r, can_frame_t *f) {
  if (r->left < CAN_HEAD_LEN) {
    return false;
  }
  f->id = get_u32(r);
  uint8_t flags = get_u8(r);
  f->ext = flags & CAN_FLAG_EXT;
  f->rtr = flags & CAN_FLAG_RTR;
  f->dlc = get_u8(r);
  if (f->dlc > CAN_MAX_DLC || r->left != f->dlc) {
    return false;
  }
  memset(f->data, 0, sizeof(f->data));
  for (int i = 0; i < f->dlc; i++) {
    f->data[i] = get_u8(r);
  }
  return true;
}

static void send_text_ack(comm_t *comm, uint8_t counter) {
  comm_send(comm, COMM_TEXT_ACK, &counter);
}

static void handle_frame(comm_t *comm, uint8_t type, uint8_t counter,
                         const uint8_t *data, size_t len) {
  int idx = type_index(type);
  if (idx < 0) {
    report(comm, COMM_ERR_TYPE, type);
    return;
  }

  // A resent text keeps its counter: confirm again, don't deliver twice
  if (type == COMM_TEXT && comm->rx_seen[idx] &&
      counter == comm->rx_counter[idx]) {
    send_text_ack(comm, counter);
    return;
  }
  if (comm->rx_seen[idx] && counter != (uint8_t)(comm->rx_counter[idx] + 1)) {
    report(comm, COMM_ERR_LOST, type);
  }
  comm->rx_counter[idx] = counter;
  comm->rx_seen[idx] = true;

  reader_t r = {data, len};
  union {
    imu_sample_t imu;
    gps_fix_t gps;
    can_frame_t can;
    comm_text_t text;
    comm_reply_t reply;
    comm_command_t command;
    comm_setting_t setting;
    uint8_t ack;
  } msg;
  bool ok = false;

  switch (type) {
  case COMM_IMU0:
  case COMM_IMU1:
  case COMM_IMU: ok = unpack_imu(&r, &msg.imu); break;
  case COMM_GPS: ok = unpack_gps(&r, &msg.gps); break;
  case COMM_CAN_RX:
  case COMM_CAN_TX: ok = unpack_can(&r, &msg.can); break;
  case COMM_TEXT:
    ok = len >= TEXT_HEAD_LEN;
    msg.text.severity = get_u8(&r);
    msg.text.source = get_u8(&r);
    msg.text.text = (const char *)r.p;
    msg.text.len = r.left;
    break;
  case COMM_REPLY:
    ok = len == REPLY_LEN;
    msg.reply.what = (comm_type_t)get_u8(&r);
    msg.reply.id = get_u32(&r);
    msg.reply.result = get_u8(&r);
    break;
  case COMM_COMMAND:
    ok = len == COMMAND_LEN || len == COMMAND_SAMPLES_LEN;
    msg.command.id = get_u8(&r);
    msg.command.samples = len == COMMAND_SAMPLES_LEN ? get_u16(&r) : 0;
    break;
  case COMM_SETTING:
    ok = len == SETTING_LEN;
    msg.setting.id = get_u8(&r);
    msg.setting.u32 = get_u32(&r);
    break;
  case COMM_TEXT_ACK:
    ok = len == TEXT_ACK_LEN;
    msg.ack = get_u8(&r);
    break;
  }

  if (!ok) {
    report(comm, COMM_ERR_LENGTH, type);
    return;
  }
  if (type == COMM_TEXT) {
    send_text_ack(comm, counter);
  }
  if (comm->handler[idx]) {
    comm->handler[idx](comm->handler_ctx[idx], (comm_type_t)type, &msg);
  }
}

static void drop(comm_t *comm, size_t n) {
  memmove(comm->rx_buf, comm->rx_buf + n, comm->rx_pos - n);
  comm->rx_pos -= n;
}

// Take every complete frame out of rx_buf
static void parse(comm_t *comm) {
  for (;;) {
    // Skip to the first marker byte
    size_t skip = 0;
    while (skip < comm->rx_pos && comm->rx_buf[skip] != COMM_MARKER_1) {
      skip++;
    }
    if (skip) {
      drop(comm, skip);
    }
    if (comm->rx_pos < 2) {
      return;
    }
    if (comm->rx_buf[1] != COMM_MARKER_2) {
      drop(comm, 1);
      continue;
    }
    if (comm->rx_pos < COMM_HEADER_LEN) {
      return;
    }

    size_t len = comm->rx_buf[OFF_LEN] | (size_t)comm->rx_buf[OFF_LEN + 1] << 8;
    size_t total = COMM_HEADER_LEN + len + COMM_CRC_LEN;
    if (comm->rx_pos < total) {
      return;
    }

    uint8_t type = comm->rx_buf[OFF_TYPE];
    uint16_t want = crc16(comm->rx_buf + CRC_START, total - CRC_START - COMM_CRC_LEN);
    uint16_t got = comm->rx_buf[total - 2] | (uint16_t)comm->rx_buf[total - 1] << 8;
    if (want != got) {
      // Rescan the same bytes starting after this marker
      report(comm, COMM_ERR_CRC, type);
      drop(comm, 1);
      continue;
    }

    handle_frame(comm, type, comm->rx_buf[OFF_COUNTER], comm->rx_buf + OFF_DATA, len);
    drop(comm, total);
  }
}

void comm_read(comm_t *comm, const uint8_t *src, size_t len) {
  for (size_t i = 0; i < len; i++) {
    comm->rx_buf[comm->rx_pos++] = src[i];
    // A frame is at most COMM_FRAME_MAX, parse keeps rx_pos below that
    parse(comm);
  }
}

// Names

const char *comm_type_name(comm_type_t type) {
  switch (type) {
  case COMM_IMU0: return "imu0";
  case COMM_IMU1: return "imu1";
  case COMM_IMU: return "imu";
  case COMM_GPS: return "gps";
  case COMM_CAN_RX: return "can rx";
  case COMM_TEXT: return "text";
  case COMM_REPLY: return "reply";
  case COMM_COMMAND: return "command";
  case COMM_CAN_TX: return "can tx";
  case COMM_SETTING: return "setting";
  case COMM_TEXT_ACK: return "text ack";
  }
  return UNKNOWN_NAME;
}

const char *comm_cmd_name(comm_cmd_t cmd) {
  switch (cmd) {
  case COMM_CMD_MAG_CAL_BEGIN: return "mag cal begin";
  case COMM_CMD_MAG_CAL_END: return "mag cal end";
  case COMM_CMD_GYRO_CAL: return "gyro cal";
  case COMM_CMD_SAVE_CAL: return "save cal";
  case COMM_CMD_REBOOT: return "reboot";
  }
  return UNKNOWN_NAME;
}

const char *comm_set_name(comm_set_t set) {
  switch (set) {
  case COMM_SET_IMU_RATE: return "imu rate";
  case COMM_SET_GPS_RATE: return "gps rate";
  case COMM_SET_BETA: return "beta";
  }
  return UNKNOWN_NAME;
}

const char *comm_result_name(comm_result_t result) {
  switch (result) {
  case COMM_OK: return "ok";
  case COMM_UNKNOWN: return "unknown";
  case COMM_OUT_OF_RANGE: return "out of range";
  case COMM_NO_DEVICE: return "no device";
  case COMM_FAILED: return "failed";
  case COMM_BUSY: return "busy";
  }
  return UNKNOWN_NAME;
}

const char *comm_error_name(comm_error_t err) {
  switch (err) {
  case COMM_ERR_CRC: return "crc";
  case COMM_ERR_LOST: return "lost";
  case COMM_ERR_LENGTH: return "length";
  case COMM_ERR_TYPE: return "type";
  }
  return UNKNOWN_NAME;
}
