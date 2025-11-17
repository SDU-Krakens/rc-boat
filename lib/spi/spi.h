#pragma once

#include "../gpio/gpio.h"
#include <stdint.h>

typedef struct {
  char *dev;
  int fd;
  uint32_t speed_hz;
  gpio_t *cs_pin;
} spi_t;

// Generic SPI functions
spi_t *spi_init(const char *dev, uint32_t speed_hz, uint8_t mode,
                uint8_t cs_pin);
void spi_close(spi_t *spi);

// Core transfer function
void spi_transfer(spi_t *spi, uint8_t *tx_buf, uint8_t *rx_buf, uint8_t len);

// Generic read/write functions (device-agnostic)
uint8_t spi_read_byte(spi_t *spi, uint8_t reg);
void spi_write_byte(spi_t *spi, uint8_t reg, uint8_t data);
void spi_write_buffer(spi_t *spi, uint8_t *data, uint8_t len);
void spi_read_buffer(spi_t *spi, uint8_t *data, uint8_t len);
void spi_write_then_read(spi_t *spi, uint8_t *tx_data, uint8_t tx_len,
                         uint8_t *rx_data, uint8_t rx_len);
