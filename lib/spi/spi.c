#include "spi.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

spi_t *spi_init(const char *dev, uint32_t speed_hz) {
  spi_t *spi = malloc(sizeof(spi_t));
  spi->fd = open(dev, O_RDWR);
  spi->speed_hz = speed_hz;

  uint8_t mode = 0;
  ioctl(spi->fd, SPI_IOC_WR_MODE, &mode);
  ioctl(spi->fd, SPI_IOC_WR_BITS_PER_WORD, &mode);
  ioctl(spi->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz);

  spi_write(spi, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
  usleep(1000);
  spi_write(spi, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

  uint8_t ver = spi_read(spi, REG_VERSION);
  if (ver != 0x12) {
    printf("spi_init: wrong version %x\n", ver);
    exit(1);
    return NULL;
  }

  return spi;
}

uint8_t spi_read(spi_t *spi, uint8_t reg) {
  uint8_t tx[2] = {(uint8_t)(reg | 0x7F), 0x00};
  uint8_t rx[2] = {0x00, 0x00};
  struct spi_ioc_transfer tr = {};
  tr.tx_buf = (unsigned long)tx;
  tr.rx_buf = (unsigned long)rx;
  tr.len = 2;
  tr.speed_hz = spi->speed_hz;
  tr.bits_per_word = 8;
  ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr);
  return rx[1];
}

void spi_write(spi_t *spi, uint8_t reg, uint8_t val) {
  uint8_t tx[2] = {(uint8_t)(reg & 0x7F), val};
  struct spi_ioc_transfer tr = {};
  tr.tx_buf = (unsigned long)tx;
  tr.len = 2;
  tr.speed_hz = spi->speed_hz;
  tr.bits_per_word = 8;
  ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr);
}

void spi_write_array(spi_t *spi, uint8_t reg, uint8_t *data, uint8_t len) {
  uint8_t hdr = reg | 0x80;
  struct spi_ioc_transfer tr[2] = {};
  tr[0].tx_buf = (unsigned long)&hdr;
  tr[0].len = 1;
  tr[0].speed_hz = spi->speed_hz;
  tr[0].bits_per_word = 8;
  tr[1].tx_buf = (unsigned long)data;
  tr[1].len = len;
  tr[1].speed_hz = spi->speed_hz;
  tr[1].bits_per_word = 8;
  ioctl(spi->fd, SPI_IOC_MESSAGE(2), &tr);
}
