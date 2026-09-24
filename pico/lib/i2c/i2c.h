#pragma once

#include "hardware/i2c.h"
#include <stddef.h>
#include <stdint.h>

// Timeout for a single blocking I2C transfer
#define I2C_TIMEOUT_US 10000

typedef struct {
  i2c_inst_t *inst;
  uint32_t sda;
  uint32_t scl;
  uint32_t baud;
} i2c_t;

i2c_t *i2c_open(i2c_inst_t *inst, uint32_t sda, uint32_t scl, uint32_t baud);
void i2c_close(i2c_t *i2c);
int i2c_write(i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t val);
int i2c_read(i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
