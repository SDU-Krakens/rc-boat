#pragma once

#include <stddef.h>
#include <stdint.h>

#define SPI_BITS_PER_WORD 8

typedef struct {
  char *dev;
  int fd;
  uint32_t speed_hz;
  uint8_t mode;
} spi_t;

spi_t *spi_open(const char *dev, uint32_t speed_hz, uint8_t mode);
void spi_close(spi_t *spi);
int spi_transfer(spi_t *spi, const uint8_t *tx, uint8_t *rx, size_t len);
