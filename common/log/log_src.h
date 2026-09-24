#pragma once

typedef enum {
  // Pico
  LOG_SRC_PICO_MAIN,
  LOG_SRC_I2C,
  LOG_SRC_UART,
  LOG_SRC_IMU,
  LOG_SRC_GPS,
  LOG_SRC_CAN,
  LOG_SRC_STORAGE,
  // Zero
  LOG_SRC_ZERO_MAIN,
  LOG_SRC_GPIO,
  LOG_SRC_SPI,
  LOG_SRC_LORA,
  // Both
  LOG_SRC_COMM,
  LOG_SRC_COUNT,
} log_src_t;

// Same numbers as the severity byte of a text message
typedef enum {
  LOG_ERR = 0,
  LOG_WARN = 1,
  LOG_INFO = 2,
  LOG_SEV_COUNT,
} log_sev_t;

const char *log_src_name(log_src_t src);
const char *log_sev_name(log_sev_t sev);
