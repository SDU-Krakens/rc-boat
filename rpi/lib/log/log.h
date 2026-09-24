#pragma once

#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LOG_TIME_MAX 32 // "2026-09-24T15:04:05.123Z"

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
  LOG_SRC_COUNT,
} log_src_t;

// Line waiting for LoRa
typedef struct log_entry {
  struct log_entry *next;
  char text[];
} log_entry_t;

typedef struct {
  FILE *fp;
  char path[CONF_LOG_PATH_MAX];
  log_entry_t *head, *tail; // oldest first
  uint32_t count;
  uint32_t dropped;
} log_t;

log_t *log_open(const char *dir);
void log_close(log_t *log);
int log_print(log_t *log, log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
int log_lora_next(log_t *log, char *buf, size_t max);
