// Zero: reads the Pico over the link, logs everything, sends telemetry and log
// lines over LoRa, takes requests from cmd.py on a Unix socket
#define _GNU_SOURCE // accept4
#include "comm.h"
#include "config.h"
#include "log.h"
#include "lora.h"
#include "uart.h"

#include <errno.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MS_PER_S 1000L
#define NS_PER_MS 1000000L
#define US_PER_MS 1000u

#define LINK_READ_CHUNK 256
#define POLL_MS 10
#define LORA_IDLE_US 10000 // LoRa thread with nothing to send

// LoRa packets: type | counter | data
#define LORA_TYPE_TELEMETRY 1
#define LORA_TYPE_LOG 2
#define LORA_HEADER_LEN 2
#define LORA_MAX_PACKET 255
#define LORA_LOG_TEXT_MAX (LORA_MAX_PACKET - LORA_HEADER_LEN)

// Telemetry: 3 IMU slots + GPS
#define IMU_SLOTS 3 // imu0, imu1, combined
#define TELEMETRY_IMU_LEN 40
#define TELEMETRY_GPS_LEN 26
#define TELEMETRY_LEN                                                          \
  (LORA_HEADER_LEN + IMU_SLOTS * TELEMETRY_IMU_LEN + TELEMETRY_GPS_LEN)

// Telemetry scales (value * scale, rounded)
#define SCALE_ACCEL 100.0f  // 0.01 m/s^2
#define SCALE_GYRO 1000.0f  // 0.001 rad/s
#define SCALE_MAG 100.0f    // 0.01 uT
#define SCALE_TEMP 100.0f   // 0.01 C
#define SCALE_QUAT 32767.0f // 1/32767
#define SCALE_ANGLE 10000.0f // 0.0001 rad
#define US_PER_TELEMETRY_MS 1000u
#define GPS_SPEED_DIV 10    // mm/s -> cm/s
#define GPS_HEADING_DIV 1000 // 1e-5 deg -> 0.01 deg
#define CAL_BITS 2
#define CAL_MASK 0x03
#define VALID_MASK 0x1F

// Command socket
#define CMD_LINE_MAX 256
#define CMD_ARGS_MAX 16
#define CMD_BACKLOG 1

#define TIME_TEXT_MAX 32

static volatile sig_atomic_t running = 1;

static lora_t *lora;
static uart_t *link_uart;
static comm_t *comm;

// Latest values, shared with the LoRa thread
static pthread_mutex_t latest_lock = PTHREAD_MUTEX_INITIALIZER;
static imu_sample_t latest_imu[IMU_SLOTS];
static bool have_imu[IMU_SLOTS];
static gps_fix_t latest_gps;
static bool have_gps;

// Command socket (reader thread only)
static int listen_fd = -1;
static int client_fd = -1;
static char client_buf[CMD_LINE_MAX];
static size_t client_len;
static bool waiting;
static comm_type_t wait_what;
static uint32_t wait_id;
static int64_t wait_deadline_ms;

static int64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * MS_PER_S + ts.tv_nsec / NS_PER_MS;
}

static void on_signal(int sig) {
  (void)sig;
  running = 0;
}

// Words for flags and codes

static void valid_words(uint32_t valid, char *buf, size_t max) {
  static const char *const names[] = {"accel", "gyro", "mag", "temp", "orient"};
  buf[0] = '\0';
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    if (valid & (1u << i)) {
      if (buf[0]) {
        strncat(buf, ",", max - strlen(buf) - 1);
      }
      strncat(buf, names[i], max - strlen(buf) - 1);
    }
  }
  if (!buf[0]) {
    snprintf(buf, max, "none");
  }
}

static const char *gps_fix_name(uint8_t fix) {
  switch (fix) {
  case 0: return "none";
  case 1: return "dead reckoning";
  case 2: return "2d";
  case 3: return "3d";
  case 4: return "gnss and dead reckoning";
  case 5: return "time only";
  }
  return "unknown";
}

static void gps_valid_words(uint8_t valid, char *buf, size_t max) {
  static const char *const names[] = {"date", "time", "resolved", "mag"};
  buf[0] = '\0';
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    if (valid & (1u << i)) {
      if (buf[0]) {
        strncat(buf, ",", max - strlen(buf) - 1);
      }
      strncat(buf, names[i], max - strlen(buf) - 1);
    }
  }
  if (!buf[0]) {
    snprintf(buf, max, "none");
  }
}

// Data log lines

static void log_imu(const char *name, const imu_sample_t *s) {
  char valid[64];
  valid_words(s->valid, valid, sizeof(valid));
  log_data(LOG_SRC_IMU,
           "%s time_us=%llu accel=%.2f,%.2f,%.2f gyro=%.3f,%.3f,%.3f "
           "mag=%.2f,%.2f,%.2f temp=%.2f quat=%.4f,%.4f,%.4f,%.4f roll=%.4f "
           "pitch=%.4f yaw=%.4f valid=%s cal=sys:%u,accel:%u,gyro:%u,mag:%u",
           name, (unsigned long long)s->timestamp_us, (double)s->accel[0],
           (double)s->accel[1], (double)s->accel[2], (double)s->gyro[0],
           (double)s->gyro[1], (double)s->gyro[2], (double)s->mag[0],
           (double)s->mag[1], (double)s->mag[2], (double)s->temp,
           (double)s->quat[0], (double)s->quat[1], (double)s->quat[2],
           (double)s->quat[3], (double)s->roll, (double)s->pitch,
           (double)s->yaw, valid, s->cal.sys, s->cal.accel, s->cal.gyro,
           s->cal.mag);
}

static void log_gps(const gps_fix_t *f) {
  char valid[64];
  gps_valid_words(f->valid, valid, sizeof(valid));
  log_data(LOG_SRC_GPS,
           "lat=%.7f lon=%.7f height=%.3f speed=%.3f heading=%.2f fix=%s "
           "satellites=%u utc=%04u-%02u-%02uT%02u:%02u:%02u valid=%s",
           f->lat / 1e7, f->lon / 1e7, f->height_mm / 1e3, f->gspeed_mm_s / 1e3,
           f->heading_1e5 / 1e5, gps_fix_name(f->fix_type), f->num_sv, f->year,
           f->month, f->day, f->hour, f->min, f->sec, valid);
}

static void log_can(const char *dir, const can_frame_t *f) {
  char data[CAN_MAX_DLC * 3 + 1] = "";
  for (int i = 0; i < f->dlc && i < CAN_MAX_DLC; i++) {
    snprintf(data + strlen(data), sizeof(data) - strlen(data), "%s%02X",
             i ? " " : "", f->data[i]);
  }
  log_data(LOG_SRC_CAN, "%s id=0x%lx %s%s data=%s", dir, (unsigned long)f->id,
           f->ext ? "extended" : "standard", f->rtr ? " rtr" : "", data);
}

// Socket answers

static void socket_answer(const char *text) {
  if (client_fd >= 0) {
    dprintf(client_fd, "%s\n", text);
  }
  log_info(LOG_SRC_ZERO_MAIN, "socket answer: %s", text);
  waiting = false;
}

static void reply_id_text(const comm_reply_t *r, char *buf, size_t max) {
  switch (r->what) {
  case COMM_COMMAND: snprintf(buf, max, "%s", comm_cmd_name((comm_cmd_t)r->id)); break;
  case COMM_SETTING: snprintf(buf, max, "%s", comm_set_name((comm_set_t)r->id)); break;
  case COMM_CAN_TX: snprintf(buf, max, "id=0x%lx", (unsigned long)r->id); break;
  default: snprintf(buf, max, "%lu", (unsigned long)r->id); break;
  }
}

// Handlers for messages from the Pico (reader thread)

static void log_rx(comm_type_t type) {
  log_info(LOG_SRC_COMM, "rx %s #%d", comm_type_name(type),
           comm_rx_counter(comm, type));
}

static int imu_slot(comm_type_t type) {
  switch (type) {
  case COMM_IMU0: return 0;
  case COMM_IMU1: return 1;
  default: return 2;
  }
}

static void on_imu(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  static const char *const names[IMU_SLOTS] = {"imu0", "imu1", "combined"};
  int slot = imu_slot(type);
  log_rx(type);

  pthread_mutex_lock(&latest_lock);
  latest_imu[slot] = *(const imu_sample_t *)msg;
  have_imu[slot] = true;
  pthread_mutex_unlock(&latest_lock);

  log_imu(names[slot], msg);
}

static void on_gps(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  log_rx(type);

  pthread_mutex_lock(&latest_lock);
  latest_gps = *(const gps_fix_t *)msg;
  have_gps = true;
  pthread_mutex_unlock(&latest_lock);

  log_gps(msg);
}

static void on_can_rx(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  log_rx(type);
  log_can("rx", msg);
}

static void on_text(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  log_rx(type);
  const comm_text_t *t = msg;
  log_src_t src = (log_src_t)t->source;
  int len = (int)t->len;

  switch (t->severity) {
  case LOG_ERR: log_err(src, "%.*s", len, t->text); break;
  case LOG_WARN: log_warn(src, "%.*s", len, t->text); break;
  default: log_info(src, "%.*s", len, t->text); break;
  }
}

static void on_reply(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  log_rx(type);
  const comm_reply_t *r = msg;
  char id[32];
  reply_id_text(r, id, sizeof(id));
  const char *result = comm_result_name((comm_result_t)r->result);

  log_info(LOG_SRC_COMM, "reply %s %s: %s", comm_type_name(r->what), id, result);
  if (waiting && r->what == wait_what && r->id == wait_id) {
    socket_answer(result);
  }
}

static void on_comm_error(void *ctx, comm_error_t err, comm_type_t type) {
  (void)ctx;
  log_warn(LOG_SRC_COMM, "%s %s", comm_error_name(err), comm_type_name(type));
}

static void link_send(void *ctx, const uint8_t *buf, size_t len) {
  uart_send(ctx, buf, len);
}

// Sends a request to the Pico and waits for its reply
static void send_request(comm_type_t type, const void *msg, uint32_t id) {
  int counter = comm_send(comm, type, msg);
  if (counter < 0) {
    socket_answer("error link send failed");
    return;
  }
  log_info(LOG_SRC_COMM, "tx %s #%d", comm_type_name(type), counter);
  waiting = true;
  wait_what = type;
  wait_id = id;
  wait_deadline_ms = now_ms() + CONF_CMD_REPLY_TIMEOUT_MS;
}

// Command socket requests

static int split(char *line, char **argv) {
  int argc = 0;
  char *save = NULL;
  for (char *tok = strtok_r(line, " \t\r", &save); tok && argc < CMD_ARGS_MAX;
       tok = strtok_r(NULL, " \t\r", &save)) {
    argv[argc++] = tok;
  }
  return argc;
}

static bool words_are(char **argv, int argc, const char *a, const char *b,
                      const char *c) {
  const char *want[] = {a, b, c};
  int n = c ? 3 : b ? 2 : 1;
  if (argc < n) {
    return false;
  }
  for (int i = 0; i < n; i++) {
    if (strcmp(argv[i], want[i]) != 0) {
      return false;
    }
  }
  return true;
}

static void request_cmd(char **argv, int argc) {
  comm_command_t c = {0};
  if (words_are(argv, argc, "mag", "cal", "begin") && argc == 3) {
    c.id = COMM_CMD_MAG_CAL_BEGIN;
  } else if (words_are(argv, argc, "mag", "cal", "end") && argc == 3) {
    c.id = COMM_CMD_MAG_CAL_END;
  } else if (words_are(argv, argc, "gyro", "cal", NULL) && argc <= 3) {
    c.id = COMM_CMD_GYRO_CAL;
    if (argc == 3) {
      char *end;
      unsigned long n = strtoul(argv[2], &end, 10);
      if (*end || n == 0 || n > UINT16_MAX) {
        socket_answer("error bad sample count");
        return;
      }
      c.samples = (uint16_t)n;
    }
  } else if (words_are(argv, argc, "save", "cal", NULL) && argc == 2) {
    c.id = COMM_CMD_SAVE_CAL;
  } else if (words_are(argv, argc, "reboot", NULL, NULL) && argc == 1) {
    c.id = COMM_CMD_REBOOT;
  } else {
    socket_answer("error unknown command");
    return;
  }
  send_request(COMM_COMMAND, &c, c.id);
}

static void request_set(char **argv, int argc) {
  comm_setting_t s = {0};
  const char *value;
  if (words_are(argv, argc, "imu", "rate", NULL) && argc == 3) {
    s.id = COMM_SET_IMU_RATE;
    value = argv[2];
  } else if (words_are(argv, argc, "gps", "rate", NULL) && argc == 3) {
    s.id = COMM_SET_GPS_RATE;
    value = argv[2];
  } else if (words_are(argv, argc, "beta", NULL, NULL) && argc == 2) {
    s.id = COMM_SET_BETA;
    value = argv[1];
  } else {
    socket_answer("error unknown setting");
    return;
  }

  char *end;
  if (s.id == COMM_SET_BETA) {
    s.f32 = strtof(value, &end);
  } else {
    s.u32 = (uint32_t)strtoul(value, &end, 10);
  }
  if (*end || end == value) {
    socket_answer("error bad value");
    return;
  }
  send_request(COMM_SETTING, &s, s.id);
}

static void request_can(char **argv, int argc) {
  can_frame_t f = {0};
  char *end;
  if (argc < 1) {
    socket_answer("error missing can id");
    return;
  }
  f.id = (uint32_t)strtoul(argv[0], &end, 16);
  if (*end) {
    socket_answer("error bad can id");
    return;
  }
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "ext") == 0) {
      f.ext = true;
    } else if (strcmp(argv[i], "rtr") == 0) {
      f.rtr = true;
    } else {
      unsigned long b = strtoul(argv[i], &end, 16);
      if (*end || b > UINT8_MAX || f.dlc >= CAN_MAX_DLC) {
        socket_answer("error bad can data");
        return;
      }
      f.data[f.dlc++] = (uint8_t)b;
    }
  }
  log_can("tx", &f);
  send_request(COMM_CAN_TX, &f, f.id);
}

static void handle_request(char *line) {
  log_info(LOG_SRC_ZERO_MAIN, "socket request: %s", line);
  char *argv[CMD_ARGS_MAX];
  int argc = split(line, argv);
  if (argc == 0) {
    socket_answer("error empty request");
  } else if (strcmp(argv[0], "cmd") == 0) {
    request_cmd(argv + 1, argc - 1);
  } else if (strcmp(argv[0], "set") == 0) {
    request_set(argv + 1, argc - 1);
  } else if (strcmp(argv[0], "can") == 0) {
    request_can(argv + 1, argc - 1);
  } else {
    socket_answer("error unknown request");
  }
}

// Next complete line from the client, if no request is waiting for its answer
static void process_client_lines(void) {
  while (!waiting) {
    char *nl = memchr(client_buf, '\n', client_len);
    if (!nl) {
      if (client_len == sizeof(client_buf)) {
        client_len = 0;
        socket_answer("error line too long");
      }
      return;
    }
    size_t n = nl - client_buf;
    char line[CMD_LINE_MAX];
    memcpy(line, client_buf, n);
    line[n] = '\0';
    memmove(client_buf, nl + 1, client_len - n - 1);
    client_len -= n + 1;
    handle_request(line);
  }
}

static void close_client(void) {
  close(client_fd);
  client_fd = -1;
  client_len = 0;
  waiting = false;
  log_info(LOG_SRC_ZERO_MAIN, "socket client disconnected");
}

static void accept_client(void) {
  int fd = accept4(listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (fd < 0) {
    return;
  }
  if (client_fd >= 0) {
    dprintf(fd, "error busy\n");
    close(fd);
    log_warn(LOG_SRC_ZERO_MAIN, "socket: second client refused");
    return;
  }
  client_fd = fd;
  log_info(LOG_SRC_ZERO_MAIN, "socket client connected");
}

static void read_client(void) {
  ssize_t n = read(client_fd, client_buf + client_len,
                   sizeof(client_buf) - client_len);
  if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
    close_client();
    return;
  }
  if (n > 0) {
    client_len += n;
    process_client_lines();
  }
}

static int open_socket(void) {
  unlink(CONF_CMD_SOCKET); // leftover from a crash

  listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (listen_fd < 0) {
    return -1;
  }
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, CONF_CMD_SOCKET, sizeof(addr.sun_path) - 1);
  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
      listen(listen_fd, CMD_BACKLOG) != 0) {
    close(listen_fd);
    listen_fd = -1;
    return -1;
  }
  return 0;
}

// LoRa thread

static int16_t scaled(float v, float scale) {
  float r = roundf(v * scale);
  if (r > INT16_MAX) {
    return INT16_MAX;
  }
  if (r < INT16_MIN) {
    return INT16_MIN;
  }
  return (int16_t)r;
}

static void put_u8(uint8_t **p, uint8_t v) { *(*p)++ = v; }

static void put_u16(uint8_t **p, uint16_t v) {
  put_u8(p, v & 0xFF);
  put_u8(p, v >> 8);
}

static void put_u32(uint8_t **p, uint32_t v) {
  put_u16(p, v & 0xFFFF);
  put_u16(p, v >> 16);
}

static void put_i16(uint8_t **p, int16_t v) { put_u16(p, (uint16_t)v); }

static void pack_imu(uint8_t **p, const imu_sample_t *s) {
  put_u32(p, (uint32_t)(s->timestamp_us / US_PER_TELEMETRY_MS));
  for (int i = 0; i < 3; i++) {
    put_i16(p, scaled(s->accel[i], SCALE_ACCEL));
  }
  for (int i = 0; i < 3; i++) {
    put_i16(p, scaled(s->gyro[i], SCALE_GYRO));
  }
  for (int i = 0; i < 3; i++) {
    put_i16(p, scaled(s->mag[i], SCALE_MAG));
  }
  put_i16(p, scaled(s->temp, SCALE_TEMP));
  for (int i = 0; i < 4; i++) {
    put_i16(p, scaled(s->quat[i], SCALE_QUAT));
  }
  put_i16(p, scaled(s->roll, SCALE_ANGLE));
  put_i16(p, scaled(s->pitch, SCALE_ANGLE));
  put_i16(p, scaled(s->yaw, SCALE_ANGLE));
  put_u8(p, s->valid & VALID_MASK);
  put_u8(p, (s->cal.sys & CAL_MASK) << (3 * CAL_BITS) |
                (s->cal.accel & CAL_MASK) << (2 * CAL_BITS) |
                (s->cal.gyro & CAL_MASK) << CAL_BITS | (s->cal.mag & CAL_MASK));
}

static void pack_gps(uint8_t **p, const gps_fix_t *f) {
  int32_t speed = f->gspeed_mm_s / GPS_SPEED_DIV;
  int32_t heading = f->heading_1e5 / GPS_HEADING_DIV;
  put_u32(p, (uint32_t)f->lat);
  put_u32(p, (uint32_t)f->lon);
  put_u32(p, (uint32_t)f->height_mm);
  put_u16(p, speed < 0 ? 0 : speed > UINT16_MAX ? UINT16_MAX : (uint16_t)speed);
  put_u16(p, heading < 0 ? 0 : heading > UINT16_MAX ? UINT16_MAX : (uint16_t)heading);
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

// Telemetry data after the LoRa header; slots without data are zero
static size_t pack_telemetry(uint8_t *buf) {
  static const imu_sample_t no_imu;
  static const gps_fix_t no_gps;
  uint8_t *p = buf;

  pthread_mutex_lock(&latest_lock);
  for (int i = 0; i < IMU_SLOTS; i++) {
    pack_imu(&p, have_imu[i] ? &latest_imu[i] : &no_imu);
  }
  pack_gps(&p, have_gps ? &latest_gps : &no_gps);
  pthread_mutex_unlock(&latest_lock);
  return p - buf;
}

static bool have_data(void) {
  pthread_mutex_lock(&latest_lock);
  bool any = have_gps;
  for (int i = 0; i < IMU_SLOTS; i++) {
    any = any || have_imu[i];
  }
  pthread_mutex_unlock(&latest_lock);
  return any;
}

static uint8_t lora_counter;

static bool send_packet(uint8_t type, uint8_t *packet, size_t len) {
  packet[0] = type;
  packet[1] = lora_counter;
  bool ok = lora_send(lora, packet, (uint8_t)len, CONF_LORA_TX_TIMEOUT_MS);
  const char *name = type == LORA_TYPE_TELEMETRY ? "telemetry" : "log";
  if (ok) {
    log_info(LOG_SRC_LORA, "tx %s #%u %zu bytes: ok", name, lora_counter, len);
  } else {
    log_warn(LOG_SRC_LORA, "tx %s #%u %zu bytes: timeout", name, lora_counter, len);
  }
  lora_counter++;
  return ok;
}

static void *lora_thread(void *arg) {
  (void)arg;
  uint8_t packet[LORA_MAX_PACKET];

  while (running) {
    bool sent = false;

    if (have_data()) {
      for (int i = 0; i < CONF_LORA_TELEMETRY_BURST && running; i++) {
        size_t len = LORA_HEADER_LEN + pack_telemetry(packet + LORA_HEADER_LEN);
        send_packet(LORA_TYPE_TELEMETRY, packet, len);
        sent = true;
      }
    }

    // Log lines for at most CONF_LORA_LOG_WINDOW_MS
    int64_t start = now_ms();
    while (running && now_ms() - start < CONF_LORA_LOG_WINDOW_MS) {
      // +1 for the terminator log_lora_next writes, not sent
      char text[LORA_LOG_TEXT_MAX + 1];
      int n = log_lora_next(text, sizeof(text));
      if (n <= 0) {
        break;
      }
      memcpy(packet + LORA_HEADER_LEN, text, n);
      send_packet(LORA_TYPE_LOG, packet, LORA_HEADER_LEN + n);
      sent = true;
    }

    if (!sent) {
      usleep(LORA_IDLE_US);
    }
  }
  return NULL;
}

// Startup helpers

static void apply_lora_settings(void) {
  lora_set_frequency(lora, CONF_LORA_FREQUENCY);
  lora_set_bandwidth(lora, CONF_LORA_BANDWIDTH);
  lora_set_spreading_factor(lora, CONF_LORA_SF);
  lora_set_coding_rate(lora, CONF_LORA_CR);
  lora_enable_crc(lora, CONF_LORA_CRC);
  lora_set_tx_power(lora, CONF_LORA_TX_POWER);
  lora_set_preamble(lora, CONF_LORA_PREAMBLE);
  lora_set_ldro(lora, CONF_LORA_LDRO);
}

// No log file: report on the terminal and over LoRa, then give up
static int fail_without_log(void) {
  char stamp[TIME_TEXT_MAX];
  time_t now = time(NULL);
  struct tm utc;
  gmtime_r(&now, &utc);
  strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc);

  char text[LORA_LOG_TEXT_MAX + 1];
  int n = snprintf(text, sizeof(text), "%s zero error can't open log file in %s: %s",
                   stamp, CONF_LOG_DIR, strerror(errno));
  fprintf(stderr, "%s\n", text);

  lora = lora_open(CONF_LORA_SPI_DEV, CONF_LORA_SPI_HZ, CONF_GPIO_CHIP,
                   CONF_LORA_RESET_PIN, CONF_LORA_DIO0_PIN);
  if (lora) {
    apply_lora_settings();
    uint8_t packet[LORA_MAX_PACKET];
    size_t len = n < 0 ? 0 : (size_t)n > LORA_LOG_TEXT_MAX ? LORA_LOG_TEXT_MAX : (size_t)n;
    memcpy(packet + LORA_HEADER_LEN, text, len);
    send_packet(LORA_TYPE_LOG, packet, LORA_HEADER_LEN + len);
    lora_close(lora);
  }
  return 1;
}

int main(void) {
  if (log_open(CONF_LOG_DIR) != 0) {
    return fail_without_log();
  }
  log_info(LOG_SRC_ZERO_MAIN, "start");

  struct sigaction sa = {.sa_handler = on_signal};
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  lora = lora_open(CONF_LORA_SPI_DEV, CONF_LORA_SPI_HZ, CONF_GPIO_CHIP,
                   CONF_LORA_RESET_PIN, CONF_LORA_DIO0_PIN);
  if (lora) {
    apply_lora_settings();
    log_info(LOG_SRC_ZERO_MAIN, "lora open");
  } else {
    log_err(LOG_SRC_ZERO_MAIN, "lora open failed");
  }

  link_uart = uart_open(CONF_LINK_DEV, CONF_LINK_BAUD);
  if (!link_uart) {
    log_err(LOG_SRC_ZERO_MAIN, "link open failed, stop");
    lora_close(lora);
    log_close();
    return 1;
  }
  comm = comm_open(link_send, link_uart);
  comm_on(comm, COMM_IMU0, on_imu, NULL);
  comm_on(comm, COMM_IMU1, on_imu, NULL);
  comm_on(comm, COMM_IMU, on_imu, NULL);
  comm_on(comm, COMM_GPS, on_gps, NULL);
  comm_on(comm, COMM_CAN_RX, on_can_rx, NULL);
  comm_on(comm, COMM_TEXT, on_text, NULL);
  comm_on(comm, COMM_REPLY, on_reply, NULL);
  comm_on_error(comm, on_comm_error, NULL);

  if (open_socket() != 0) {
    log_err(LOG_SRC_ZERO_MAIN, "can't open socket %s: %s", CONF_CMD_SOCKET,
            strerror(errno));
  } else {
    log_info(LOG_SRC_ZERO_MAIN, "socket %s open", CONF_CMD_SOCKET);
  }

  pthread_t lora_tid;
  bool lora_started = lora && pthread_create(&lora_tid, NULL, lora_thread, NULL) == 0;
  log_info(LOG_SRC_ZERO_MAIN, "running");

  // Reader thread: link and command socket
  while (running) {
    struct pollfd pfd[3] = {
        {.fd = link_uart->fd, .events = POLLIN},
        {.fd = listen_fd, .events = POLLIN},
        {.fd = client_fd, .events = POLLIN},
    };
    poll(pfd, 3, POLL_MS);

    if (pfd[0].revents & POLLIN) {
      uint8_t buf[LINK_READ_CHUNK];
      size_t n;
      while ((n = uart_read(link_uart, buf, sizeof(buf))) > 0) {
        comm_read(comm, buf, n);
      }
      // A reply may have freed the socket for the next queued request
      if (client_fd >= 0) {
        process_client_lines();
      }
    }
    if (listen_fd >= 0 && (pfd[1].revents & POLLIN)) {
      accept_client();
    }
    if (client_fd >= 0 && (pfd[2].revents & (POLLIN | POLLHUP))) {
      read_client();
    }
    if (waiting && now_ms() > wait_deadline_ms) {
      socket_answer("timeout");
      process_client_lines();
    }
  }

  log_info(LOG_SRC_ZERO_MAIN, "stop");
  if (lora_started) {
    pthread_join(lora_tid, NULL);
  }
  if (client_fd >= 0) {
    close_client();
  }
  if (listen_fd >= 0) {
    close(listen_fd);
    unlink(CONF_CMD_SOCKET);
  }
  comm_close(comm);
  uart_close(link_uart);
  lora_close(lora);
  log_close();
  return 0;
}
