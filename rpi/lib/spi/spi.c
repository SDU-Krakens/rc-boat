#include "spi.h"
#include "config.h"

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

spi_t *spi_open(const char *dev, uint32_t speed_hz, uint8_t mode) {
  spi_t *spi = malloc(sizeof(spi_t));
  if (!spi) {
    return NULL;
  }

  spi->fd = open(dev, O_RDWR | O_CLOEXEC);
  if (spi->fd < 0) {
#ifdef CONF_DEBUG
    printf("spi_open: can't open %s\n", dev);
#endif
    free(spi);
    return NULL;
  }
  spi->dev = strdup(dev);
  spi->speed_hz = speed_hz;
  spi->mode = mode;

  uint8_t bits = SPI_BITS_PER_WORD;
  if (ioctl(spi->fd, SPI_IOC_WR_MODE, &mode) < 0 ||
      ioctl(spi->fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
      ioctl(spi->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
#ifdef CONF_DEBUG
    printf("spi_open: can't configure %s\n", dev);
#endif
    spi_close(spi);
    return NULL;
  }

  return spi;
}

void spi_close(spi_t *spi) {
  if (!spi) {
    return;
  }
  if (spi->fd >= 0) {
    close(spi->fd);
  }
  free(spi->dev);
  free(spi);
}

int spi_transfer(spi_t *spi, const uint8_t *tx, uint8_t *rx, size_t len) {
  // Chip select is driven by the kernel (CE0) for the whole transfer
  struct spi_ioc_transfer tr = {
      .tx_buf = (unsigned long)tx,
      .rx_buf = (unsigned long)rx,
      .len = len,
      .speed_hz = spi->speed_hz,
      .bits_per_word = SPI_BITS_PER_WORD,
  };

  if (ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
    return -1;
  }
  return 0;
}
