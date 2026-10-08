#include "log_src.h"

#define UNKNOWN_NAME "unknown"

static const char *const src_names[LOG_SRC_COUNT] = {
    [LOG_SRC_PICO_MAIN] = "pico", [LOG_SRC_I2C] = "i2c",
    [LOG_SRC_UART] = "uart",      [LOG_SRC_IMU] = "imu",
    [LOG_SRC_GPS] = "gps",        [LOG_SRC_CAN] = "can",
    [LOG_SRC_STORAGE] = "storage", [LOG_SRC_ZERO_MAIN] = "zero",
    [LOG_SRC_GPIO] = "gpio",      [LOG_SRC_SPI] = "spi",
    [LOG_SRC_LORA] = "lora",      [LOG_SRC_COMM] = "comm",
};

static const char *const sev_names[LOG_SEV_COUNT] = {
    [LOG_ERR] = "error",
    [LOG_WARN] = "warning",
    [LOG_INFO] = "info",
};

const char *log_src_name(log_src_t src) {
  return (unsigned)src < LOG_SRC_COUNT ? src_names[src] : UNKNOWN_NAME;
}

const char *log_sev_name(log_sev_t sev) {
  return (unsigned)sev < LOG_SEV_COUNT ? sev_names[sev] : UNKNOWN_NAME;
}
