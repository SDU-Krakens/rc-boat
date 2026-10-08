// Pico sensor hub: reads the IMUs, GPS and CAN and sends everything to the Zero
#include "can.h"
#include "comm.h"
#include "config.h"
#include "gps.h"
#include "i2c.h"
#include "imu.h"
#include "log.h"
#include "madgwick.h"
#include "storage.h"
#include "uart.h"

#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include <string.h>

#define US_PER_S 1000000u
#define LINK_READ_CHUNK 64
#define IMU_COUNT 2

// Settings in flash, one entry each (calibration uses tags 1 and 2)
#define TAG_IMU_RATE 3
#define TAG_GPS_RATE 4
#define TAG_BETA 5

// Allowed setting values
#define IMU_RATE_MIN 1
#define IMU_RATE_MAX 100
#define GPS_RATE_MIN 1
#define GPS_RATE_MAX 10
#define BETA_MAX 1.0f

typedef struct {
  uint32_t imu_rate;
  uint32_t gps_rate;
  float beta;
} settings_t;

static uart_t *link_uart;
static comm_t *comm;
static storage_t *st;
static i2c_t *bus[IMU_COUNT];
static imu_t *imu[IMU_COUNT]; // index = I2C bus
static imu_t *icm;            // the ICM-20948 of imu[], NULL if none
static gps_t *gps;
static can_t *can;
static madgwick_t combined_filter;
static settings_t settings;

static bool mag_cal_running;
static bool gyro_cal_running;
static bool gyro_cal_reply; // started by a command, reply when done

static void link_send(void *ctx, const uint8_t *buf, size_t len) {
  uart_send(ctx, buf, len);
}

static void read_link(void) {
  uint8_t buf[LINK_READ_CHUNK];
  size_t n;
  while ((n = uart_read(link_uart, buf, sizeof(buf))) > 0) {
    comm_read(comm, buf, n);
  }
}

static void reply(comm_type_t what, uint32_t id, comm_result_t result) {
  comm_reply_t r = {what, id, result};
  comm_send(comm, COMM_REPLY, &r);
}

static const char *imu_name(const imu_t *i) {
  return i->type == IMU_ICM20948 ? "icm20948" : "bno055";
}

// IMU detection

static int open_pair(imu_type_t t0, imu_type_t t1) {
  imu[0] = imu_open(t0, bus[0], &IMU_ROT_IDENTITY);
  imu[1] = imu_open(t1, bus[1], &IMU_ROT_IDENTITY);
  return (imu[0] != NULL) + (imu[1] != NULL);
}

static void close_pair(void) {
  for (int i = 0; i < IMU_COUNT; i++) {
    imu_close(imu[i]);
    imu[i] = NULL;
  }
}

static void detect_imus(void) {
  int found = open_pair(IMU_BNO055, IMU_ICM20948);
  if (found < IMU_COUNT) {
    close_pair();
    int swapped = open_pair(IMU_ICM20948, IMU_BNO055);
    if (swapped < found) {
      close_pair();
      open_pair(IMU_BNO055, IMU_ICM20948);
    }
  }

  icm = NULL;
  for (int i = 0; i < IMU_COUNT; i++) {
    if (!imu[i]) {
      log_warn(LOG_SRC_PICO_MAIN, "imu%d: nothing on i2c%d", i, i);
      continue;
    }
    log_info(LOG_SRC_PICO_MAIN, "imu%d: %s on i2c%d", i, imu_name(imu[i]), i);
    if (imu[i]->type == IMU_ICM20948) {
      icm = imu[i];
    }
  }
}

// Settings

static void load_settings(void) {
  settings.imu_rate = CONF_IMU_SAMPLE_HZ;
  settings.gps_rate = CONF_GPS_RATE_HZ;
  settings.beta = CONF_MADGWICK_BETA;

  uint32_t u;
  float f;
  if (storage_load(st, TAG_IMU_RATE, &u, sizeof(u)) == 0) {
    settings.imu_rate = u;
  }
  if (storage_load(st, TAG_GPS_RATE, &u, sizeof(u)) == 0) {
    settings.gps_rate = u;
  }
  if (storage_load(st, TAG_BETA, &f, sizeof(f)) == 0) {
    settings.beta = f;
  }
  log_info(LOG_SRC_PICO_MAIN, "settings: imu rate %lu Hz, gps rate %lu Hz, beta %.3f",
           (unsigned long)settings.imu_rate, (unsigned long)settings.gps_rate,
           (double)settings.beta);
}

static int apply_imu_rate(uint32_t hz) {
  int rc = 0;
  for (int i = 0; i < IMU_COUNT; i++) {
    if (imu[i] && imu_set_rate(imu[i], hz) != 0) {
      rc = -1;
    }
  }
  return rc;
}

static void apply_beta(float beta) {
  for (int i = 0; i < IMU_COUNT; i++) {
    if (imu[i]) {
      imu_set_beta(imu[i], beta);
    }
  }
  combined_filter.beta = beta;
}

// Handlers for messages from the Zero

static comm_result_t do_command(const comm_command_t *c) {
  switch (c->id) {
  case COMM_CMD_MAG_CAL_BEGIN:
    if (!icm) {
      return COMM_NO_DEVICE;
    }
    if (mag_cal_running || gyro_cal_running) {
      return COMM_BUSY;
    }
    if (imu_cal_mag_begin(icm) != 0) {
      return COMM_FAILED;
    }
    mag_cal_running = true;
    return COMM_OK;

  case COMM_CMD_MAG_CAL_END:
    if (!icm) {
      return COMM_NO_DEVICE;
    }
    if (!mag_cal_running) {
      return COMM_FAILED;
    }
    mag_cal_running = false;
    return imu_cal_mag_end(icm) == 0 ? COMM_OK : COMM_FAILED;

  case COMM_CMD_GYRO_CAL: {
    if (!icm) {
      return COMM_NO_DEVICE;
    }
    if (mag_cal_running || gyro_cal_running) {
      return COMM_BUSY;
    }
    uint32_t samples = c->samples ? c->samples : CONF_GYRO_CAL_SAMPLES;
    if (imu_cal_gyro_begin(icm, samples) != 0) {
      return COMM_FAILED;
    }
    gyro_cal_running = true;
    gyro_cal_reply = true;
    log_info(LOG_SRC_PICO_MAIN, "gyro cal started, %lu samples",
             (unsigned long)samples);
    return COMM_OK; // not replied yet, see the main loop
  }

  case COMM_CMD_SAVE_CAL: {
    comm_result_t result = COMM_OK;
    for (int i = 0; i < IMU_COUNT; i++) {
      if (imu[i] && imu_cal_save(imu[i], st) != 0) {
        result = COMM_FAILED;
      }
    }
    return result;
  }

  case COMM_CMD_REBOOT:
    return COMM_OK;
  }
  return COMM_UNKNOWN;
}

static void on_command(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  (void)type;
  const comm_command_t *c = msg;
  comm_result_t result = do_command(c);

  // Gyro cal answers when it is done
  if (c->id == COMM_CMD_GYRO_CAL && result == COMM_OK) {
    return;
  }

  reply(COMM_COMMAND, c->id, result);
  log_info(LOG_SRC_PICO_MAIN, "command %s: %s",
           comm_cmd_name((comm_cmd_t)c->id), comm_result_name(result));

  if (c->id == COMM_CMD_REBOOT) {
    log_poll();
    uart_tx_wait_blocking(link_uart->inst);
    watchdog_reboot(0, 0, 0);
    for (;;) {
    }
  }
}

static comm_result_t do_setting(const comm_setting_t *s) {
  switch (s->id) {
  case COMM_SET_IMU_RATE: {
    uint32_t old = settings.imu_rate;
    if (s->u32 < IMU_RATE_MIN || s->u32 > IMU_RATE_MAX) {
      return COMM_OUT_OF_RANGE;
    }
    if (apply_imu_rate(s->u32) != 0) {
      apply_imu_rate(old);
      return COMM_FAILED;
    }
    if (storage_save(st, TAG_IMU_RATE, &s->u32, sizeof(s->u32)) != 0) {
      apply_imu_rate(old);
      return COMM_FAILED;
    }
    settings.imu_rate = s->u32;
    return COMM_OK;
  }

  case COMM_SET_GPS_RATE: {
    uint32_t old = settings.gps_rate;
    if (s->u32 < GPS_RATE_MIN || s->u32 > GPS_RATE_MAX) {
      return COMM_OUT_OF_RANGE;
    }
    if (!gps) {
      return COMM_NO_DEVICE;
    }
    if (gps_set_rate(gps, s->u32) != 0) {
      return COMM_FAILED;
    }
    if (storage_save(st, TAG_GPS_RATE, &s->u32, sizeof(s->u32)) != 0) {
      gps_set_rate(gps, old);
      return COMM_FAILED;
    }
    settings.gps_rate = s->u32;
    return COMM_OK;
  }

  case COMM_SET_BETA: {
    float old = settings.beta;
    if (!(s->f32 > 0.0f && s->f32 <= BETA_MAX)) {
      return COMM_OUT_OF_RANGE;
    }
    apply_beta(s->f32);
    if (storage_save(st, TAG_BETA, &s->f32, sizeof(s->f32)) != 0) {
      apply_beta(old);
      return COMM_FAILED;
    }
    settings.beta = s->f32;
    return COMM_OK;
  }
  }
  return COMM_UNKNOWN;
}

static void on_setting(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  (void)type;
  const comm_setting_t *s = msg;
  comm_result_t result = do_setting(s);
  reply(COMM_SETTING, s->id, result);
  log_info(LOG_SRC_PICO_MAIN, "setting %s: %s", comm_set_name((comm_set_t)s->id),
           comm_result_name(result));
}

static void on_can_tx(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  (void)type;
  const can_frame_t *f = msg;
  comm_result_t result;

  if (!can) {
    result = COMM_NO_DEVICE;
  } else if (f->dlc > CAN_MAX_DLC) {
    result = COMM_FAILED;
  } else {
    // can_send fails only when the transmit queue is full
    result = can_send(can, f) == 0 ? COMM_OK : COMM_BUSY;
  }

  reply(COMM_CAN_TX, f->id, result);
  log_info(LOG_SRC_CAN, "tx id 0x%lx dlc %u: %s", (unsigned long)f->id, f->dlc,
           comm_result_name(result));
}

static void on_comm_error(void *ctx, comm_error_t err, comm_type_t type) {
  (void)ctx;
  log_warn(LOG_SRC_COMM, "%s %s", comm_error_name(err), comm_type_name(type));
}

// Main loop steps

static void calibration_step(void) {
  if (mag_cal_running) {
    imu_cal_mag_update(icm);
  }
  if (!gyro_cal_running) {
    return;
  }

  int rc = imu_cal_gyro_update(icm);
  if (rc == 0) {
    return;
  }
  gyro_cal_running = false;
  comm_result_t result = rc == 1 ? COMM_OK : COMM_FAILED;
  log_info(LOG_SRC_PICO_MAIN, "gyro cal: %s", comm_result_name(result));
  if (gyro_cal_reply) {
    reply(COMM_COMMAND, COMM_CMD_GYRO_CAL, result);
    gyro_cal_reply = false;
  }
}

static void imu_step(void) {
  static const comm_type_t types[IMU_COUNT] = {COMM_IMU0, COMM_IMU1};
  imu_sample_t s[IMU_COUNT];
  bool have[IMU_COUNT];

  for (int i = 0; i < IMU_COUNT; i++) {
    have[i] = imu[i] && imu_read(imu[i], &s[i]) == 0;
  }
  calibration_step();

  imu_sample_t combined;
  if (have[0] && have[1]) {
    imu_average(&s[0], &s[1], &combined);
    if (combined.valid & IMU_VALID_MAG) {
      madgwick_update(&combined_filter, combined.gyro, combined.accel,
                      combined.mag, 1.0f / settings.imu_rate);
      combined.valid |= IMU_VALID_ORIENT;
    }
    memcpy(combined.quat, combined_filter.q, sizeof(combined.quat));
    madgwick_euler(&combined_filter, &combined.roll, &combined.pitch,
                   &combined.yaw);
    combined.cal = (imu_cal_status_t){0};
  } else if (have[0] || have[1]) {
    combined = have[0] ? s[0] : s[1];
  }

  for (int i = 0; i < IMU_COUNT; i++) {
    if (have[i]) {
      comm_send(comm, types[i], &s[i]);
    }
  }
  if (have[0] || have[1]) {
    comm_send(comm, COMM_IMU, &combined);
  }
}

static void gps_step(void) {
  if (gps && gps_poll(gps)) {
    comm_send(comm, COMM_GPS, &gps->fix);
  }
}

static void can_step(void) {
  can_frame_t frame;
  while (can && can_receive(can, &frame)) {
    comm_send(comm, COMM_CAN_RX, &frame);
  }
}

int main(void) {
  // Link first, so every driver message after this reaches the Zero
  link_uart = uart_open(uart_get_instance(CONF_LINK_UART), CONF_LINK_TX_PIN,
                        CONF_LINK_RX_PIN, CONF_LINK_BAUD);
  comm = comm_open(link_send, link_uart);
  log_open(comm);
  comm_on_error(comm, on_comm_error, NULL);
  log_info(LOG_SRC_PICO_MAIN, "start");

  st = storage_open();
  load_settings();

  bus[0] = i2c_open(i2c_get_instance(CONF_IMU0_I2C), CONF_IMU0_SDA,
                    CONF_IMU0_SCL, CONF_I2C_BAUD);
  bus[1] = i2c_open(i2c_get_instance(CONF_IMU1_I2C), CONF_IMU1_SDA,
                    CONF_IMU1_SCL, CONF_I2C_BAUD);
  detect_imus();

  for (int i = 0; i < IMU_COUNT; i++) {
    if (imu[i] && imu_cal_load(imu[i], st) != 0) {
      log_warn(LOG_SRC_PICO_MAIN, "imu%d: no saved calibration", i);
    }
  }
  // ICM without a saved gyro bias: measure it inside the loop
  if (icm && icm->icm.cal.n == 0 &&
      imu_cal_gyro_begin(icm, CONF_GYRO_CAL_SAMPLES) == 0) {
    gyro_cal_running = true;
    log_info(LOG_SRC_PICO_MAIN, "gyro cal at boot, keep the boat still");
  }

  madgwick_open(&combined_filter, settings.beta);
  apply_imu_rate(settings.imu_rate);
  apply_beta(settings.beta);

  uart_t *gps_uart = uart_open(uart_get_instance(CONF_GPS_UART), CONF_GPS_TX_PIN,
                               CONF_GPS_RX_PIN, CONF_GPS_BAUD);
  gps = gps_open(gps_uart);
  if (gps) {
    gps_set_rate(gps, settings.gps_rate);
  }

  can = can_open(CONF_CAN_PIO, CONF_CAN_RX_PIN, CONF_CAN_TX_PIN,
                 CONF_CAN_BITRATE);

  comm_on(comm, COMM_COMMAND, on_command, NULL);
  comm_on(comm, COMM_SETTING, on_setting, NULL);
  comm_on(comm, COMM_CAN_TX, on_can_tx, NULL);
  log_info(LOG_SRC_PICO_MAIN, "running");

  absolute_time_t next = get_absolute_time();
  for (;;) {
    // Zero not confirming logs: pause until there is room again
    while (log_full()) {
      read_link();
      log_poll();
    }

    imu_step();
    gps_step();
    can_step();
    read_link();
    log_poll();

    // Fixed time slots; if a pass overran, start counting from now
    next = delayed_by_us(next, US_PER_S / settings.imu_rate);
    if (absolute_time_diff_us(get_absolute_time(), next) < 0) {
      next = get_absolute_time();
    }
    sleep_until(next);
  }
}
