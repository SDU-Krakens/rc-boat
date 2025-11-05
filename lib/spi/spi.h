#pragma once

#include <stdint.h>

#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_PA_DAC 0x4D
#define REG_LNA 0x0C
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_FIFO_TX_BASE_ADDR 0x0E
#define REG_FIFO_RX_BASE_ADDR 0x0F
#define REG_IRQ_FLAGS 0x12
#define REG_MODEM_CONFIG1 0x1D
#define REG_MODEM_CONFIG2 0x1E
#define REG_MODEM_CONFIG3 0x26
#define REG_PAYLOAD_LENGTH 0x22
#define REG_DIO_MAPPING1 0x40
#define REG_VERSION 0x42

// --- Mode bits ---
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03

#define IRQ_TX_DONE_MASK 0x08

typedef struct {
  char *dev;
  int fd;
  uint32_t speed_hz;
} spi_t;

spi_t *spi_init(const char *dev, uint32_t speed_hz);
void spi_close(spi_t *spi);
uint8_t spi_read(spi_t *spi, uint8_t reg);
void spi_write(spi_t *spi, uint8_t reg, uint8_t data);
void spi_write_array(spi_t *spi, uint8_t reg, uint8_t *data, uint8_t len);
