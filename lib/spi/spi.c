#include "spi.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

spi_t *spi_init(const char *dev, uint32_t speed_hz, uint8_t mode,
                uint8_t cs_pin) {
  spi_t *spi = malloc(sizeof(spi_t));
  spi->fd = open(dev, O_RDWR);
  if (spi->fd < 0) {
    printf("spi_init: failed to open device\n");
    free(spi);
    return NULL;
  }

  spi->speed_hz = speed_hz;
  spi->cs_pin = gpio_open(cs_pin);
  gpio_set_direction(spi->cs_pin, GPIO_OUTPUT);

  if (ioctl(spi->fd, SPI_IOC_WR_MODE, &mode) < 0) {
    printf("spi_init: can't set spi mode\n");
  }

  uint8_t bits = 8;
  if (ioctl(spi->fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
    printf("spi_init: can't set bits per word\n");
  }

  if (ioctl(spi->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
    printf("spi_init: can't set max speed hz\n");
  }

  return spi;
}

void spi_close(spi_t *spi) {
  if (spi) {
    if (spi->fd >= 0)
      close(spi->fd);
    if (spi->cs_pin)
      gpio_close(spi->cs_pin); // Assuming gpio_close exists
    free(spi);
  }
}

// Core transfer function - remains the same
void spi_transfer(spi_t *spi, uint8_t *tx_buf, uint8_t *rx_buf, uint8_t len) {
  if (!spi || !tx_buf)
    return;

  gpio_set_value(spi->cs_pin, 0);
  struct spi_ioc_transfer tr = {
      .tx_buf = (unsigned long)tx_buf,
      .rx_buf = (unsigned long)rx_buf,
      .len = len,
      .speed_hz = spi->speed_hz,
      .bits_per_word = 8,
      .delay_usecs = 0,
  };

  ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr);
  gpio_set_value(spi->cs_pin, 1);
}

// Generic read byte from register
uint8_t spi_read_byte(spi_t *spi, uint8_t reg) {
  uint8_t tx[2] = {reg, 0};
  uint8_t rx[2] = {0};
  spi_transfer(spi, tx, rx, 2);
  return rx[1];
}

// Generic write byte to register
void spi_write_byte(spi_t *spi, uint8_t reg, uint8_t data) {
  uint8_t tx[2] = {reg, data};
  uint8_t rx[2] = {0};
  spi_transfer(spi, tx, rx, 2);
}

// Write buffer without register addressing
void spi_write_buffer(spi_t *spi, uint8_t *data, uint8_t len) {
  if (!spi || !data || len == 0)
    return;

  gpio_set_value(spi->cs_pin, 0);
  struct spi_ioc_transfer tr = {
      .tx_buf = (unsigned long)data,
      .rx_buf = 0,
      .len = len,
      .speed_hz = spi->speed_hz,
      .bits_per_word = 8,
  };
  ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr);
  gpio_set_value(spi->cs_pin, 1);
}

// Read buffer without register addressing
void spi_read_buffer(spi_t *spi, uint8_t *data, uint8_t len) {
  if (!spi || !data || len == 0)
    return;

  uint8_t tx[len];
  for (int i = 0; i < len; i++)
    tx[i] = 0; // Dummy data for read

  gpio_set_value(spi->cs_pin, 0);
  struct spi_ioc_transfer tr = {
      .tx_buf = (unsigned long)tx,
      .rx_buf = (unsigned long)data,
      .len = len,
      .speed_hz = spi->speed_hz,
      .bits_per_word = 8,
  };
  ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr);
  gpio_set_value(spi->cs_pin, 1);
}

// Write then read in single transaction
void spi_write_then_read(spi_t *spi, uint8_t *tx_data, uint8_t tx_len,
                         uint8_t *rx_data, uint8_t rx_len) {
  if (!spi || !tx_data)
    return;

  gpio_set_value(spi->cs_pin, 0);

  if (tx_len > 0) {
    struct spi_ioc_transfer tr_tx = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = 0,
        .len = tx_len,
        .speed_hz = spi->speed_hz,
        .bits_per_word = 8,
    };
    ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr_tx);
  }

  if (rx_len > 0 && rx_data) {
    uint8_t tx_dummy[rx_len];
    for (int i = 0; i < rx_len; i++)
      tx_dummy[i] = 0;

    struct spi_ioc_transfer tr_rx = {
        .tx_buf = (unsigned long)tx_dummy,
        .rx_buf = (unsigned long)rx_data,
        .len = rx_len,
        .speed_hz = spi->speed_hz,
        .bits_per_word = 8,
    };
    ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr_rx);
  }

  gpio_set_value(spi->cs_pin, 1);
}
