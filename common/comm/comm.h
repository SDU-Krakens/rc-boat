#pragma once

#include "log_src.h"
#include "types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Frame: AA 55 | len (2) | type | counter | data | crc (2), little-endian
#define COMM_MARKER_1 0xAA
#define COMM_MARKER_2 0x55
#define COMM_HEADER_LEN 6 // marker (2) + len (2) + type + counter
#define COMM_CRC_LEN 2
#define COMM_DATA_MAX 65535
#define COMM_FRAME_MAX (COMM_HEADER_LEN + COMM_DATA_MAX + COMM_CRC_LEN)
#define COMM_TYPE_COUNT 11

typedef enum {
  // Pico -> Zero
  COMM_IMU0 = 0x01,
  COMM_IMU1 = 0x02,
  COMM_IMU = 0x03,
  COMM_GPS = 0x04,
  COMM_CAN_RX = 0x05,
  COMM_TEXT = 0x06,
  COMM_REPLY = 0x07,
  // Zero -> Pico
  COMM_COMMAND = 0x81,
  COMM_CAN_TX = 0x82,
  COMM_SETTING = 0x83,
  COMM_TEXT_ACK = 0x84,
} comm_type_t;

typedef enum {
  COMM_CMD_MAG_CAL_BEGIN = 1,
  COMM_CMD_MAG_CAL_END = 2,
  COMM_CMD_GYRO_CAL = 3,
  COMM_CMD_SAVE_CAL = 4,
  COMM_CMD_REBOOT = 5,
} comm_cmd_t;

typedef enum {
  COMM_SET_IMU_RATE = 1,
  COMM_SET_GPS_RATE = 2,
  COMM_SET_BETA = 3,
} comm_set_t;

typedef enum {
  COMM_OK = 0,
  COMM_UNKNOWN = 1,
  COMM_OUT_OF_RANGE = 2,
  COMM_NO_DEVICE = 3,
  COMM_FAILED = 4,
  COMM_BUSY = 5,
} comm_result_t;

typedef enum {
  COMM_ERR_CRC,    // checksum wrong, frame dropped and bytes rescanned
  COMM_ERR_LOST,   // counter jumped, messages missing
  COMM_ERR_LENGTH, // data length wrong for the type
  COMM_ERR_TYPE,   // unknown type byte
} comm_error_t;

typedef struct {
  uint8_t id;
  uint16_t samples; // gyro cal only, 0 = CONF default
} comm_command_t;

typedef struct {
  uint8_t id;
  union {
    uint32_t u32;
    float f32;
  };
} comm_setting_t;

typedef struct {
  comm_type_t what; // COMM_COMMAND, COMM_SETTING or COMM_CAN_TX
  uint32_t id;      // command / setting number, or the full CAN id
  uint8_t result;
} comm_reply_t;

typedef struct {
  uint8_t severity;
  uint8_t source;
  const char *text; // points into the rx buffer, valid during the callback only
  size_t len;
} comm_text_t;

typedef void (*comm_send_fn)(void *ctx, const uint8_t *buf, size_t len);
typedef void (*comm_handler_fn)(void *ctx, comm_type_t type, const void *msg);
typedef void (*comm_error_fn)(void *ctx, comm_error_t err, comm_type_t type);

typedef struct {
  comm_send_fn send;
  void *send_ctx;
  comm_handler_fn handler[COMM_TYPE_COUNT];
  void *handler_ctx[COMM_TYPE_COUNT];
  comm_error_fn on_error;
  void *error_ctx;
  uint8_t tx_counter[COMM_TYPE_COUNT];
  uint8_t rx_counter[COMM_TYPE_COUNT];
  bool rx_seen[COMM_TYPE_COUNT];
  uint8_t rx_buf[COMM_FRAME_MAX];
  size_t rx_pos;
} comm_t;

comm_t *comm_open(comm_send_fn send, void *send_ctx);
void comm_close(comm_t *comm);
// Returns the counter the message was sent with (0-255), -1 on error
int comm_send(comm_t *comm, comm_type_t type, const void *msg);
int comm_send_text(comm_t *comm, log_sev_t sev, log_src_t src, const char *fmt,
                   ...) __attribute__((format(printf, 4, 5)));
int comm_resend_text(comm_t *comm, uint8_t counter, log_sev_t sev,
                     log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));
void comm_on(comm_t *comm, comm_type_t type, comm_handler_fn fn, void *ctx);
void comm_on_error(comm_t *comm, comm_error_fn fn, void *ctx);
void comm_read(comm_t *comm, const uint8_t *src, size_t len);
// Counter of the last received message of that type, -1 for an unknown type
int comm_rx_counter(comm_t *comm, comm_type_t type);

// Words for the log file
const char *comm_type_name(comm_type_t type);
const char *comm_cmd_name(comm_cmd_t cmd);
const char *comm_set_name(comm_set_t set);
const char *comm_result_name(comm_result_t result);
const char *comm_error_name(comm_error_t err);
