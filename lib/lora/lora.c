#include "lora.h"
#include "../gpio/gpio.h"
#include "../spi/spi.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

// In lora.c - keep these functions but implement LoRa-specific addressing
uint8_t lora_read_reg(lora_t *lora, uint8_t reg) {
  uint8_t tx[2] = {reg & 0x7F, 0}; // LoRa-specific: clear MSB for read
  uint8_t rx[2] = {0};
  spi_transfer(lora->spi, tx, rx, 2);
  printf("lora read: %x %x\n", reg, rx[1]);
  return rx[1];
}

void lora_write_reg(lora_t *lora, uint8_t reg, uint8_t val) {
  uint8_t tx[2] = {reg | 0x80, val}; // LoRa-specific: set MSB for write
  uint8_t rx[2] = {0};
  spi_transfer(lora->spi, tx, rx, 2);
  printf("lora write: %x %x\n", reg, val);
}

// Setup & initialization
lora_t *lora_init(const char *spi_dev, uint32_t spi_speed_hz,
                  uint8_t reset_pin) {
  lora_t *lora = malloc(sizeof(lora_t));
  if (!lora) {
    return NULL;
  }

  // Setup GPIO
  lora->reset = gpio_open(reset_pin);
  gpio_set_direction(lora->reset, GPIO_OUTPUT);

  gpio_set_value(lora->reset, 0);
  usleep(10000);
  gpio_set_value(lora->reset, 1);
  usleep(10000);

  lora->spi = spi_init(spi_dev, spi_speed_hz, 0, 27);
  if (!lora->spi) {
    free(lora);
    return NULL;
  }

  uint8_t ver = lora_read_reg(lora, REG_VERSION);
  if (ver != 0x12) {
    printf("spi_init: wrong version %x\n", ver);
    exit(1);
    return NULL;
  }

  lora->frequency = 433E6;
  lora->bandwidth = 125E3;
  lora->spreading_factor = 7;
  lora->coding_rate = 5;
  lora->crc_enabled = true;
  lora->tx_power = 17;

  lora_write_reg(lora, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
  usleep(10000);
  lora_write_reg(lora, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
  usleep(10000);

  lora_write_reg(lora, REG_PREAMBLE_MSB, 0x00);
  lora_write_reg(lora, REG_PREAMBLE_LSB, 0x08);
  lora_write_reg(lora, REG_FIFO_TX_BASE_ADDR, 0x80);
  lora_write_reg(lora, REG_FIFO_RX_BASE_ADDR, 0x00);
  lora_write_reg(lora, REG_LNA, 0x23);
  lora_write_reg(lora, REG_MODEM_CONFIG3, 0x04);

  return lora;
}

void lora_end(lora_t *lora) {
  if (lora->spi->fd >= 0)
    close(lora->spi->fd);
}

// Configuration (call before begin)
void lora_set_frequency(lora_t *lora, uint32_t frequency) {
  lora->frequency = frequency;
  uint64_t frf = ((uint64_t)frequency << 19) / 32000000;
  lora_write_reg(lora, REG_FRF_MSB, (frf >> 16));
  lora_write_reg(lora, REG_FRF_MID, (frf >> 8));
  lora_write_reg(lora, REG_FRF_LSB, (frf >> 0));
}

void lora_set_spreading_factor(lora_t *lora, uint16_t sf) {
  if (sf < 6)
    sf = 6;
  if (sf > 12)
    sf = 12;
  lora->spreading_factor = sf;

  uint8_t config = lora_read_reg(lora, REG_MODEM_CONFIG2);
  config = (config & 0x0F) | ((sf << 4) & 0xF0);

  lora_write_reg(lora, REG_MODEM_CONFIG2, config);
}

void lora_set_coding_rate(lora_t *lora, uint16_t cr) {
  if (cr < 5)
    cr = 5;
  if (cr > 8)
    cr = 8;
  lora->coding_rate = cr;
  cr -= 4;
  uint8_t config = lora_read_reg(lora, REG_MODEM_CONFIG1);
  config = (config & 0xF1) | (cr << 1);

  lora_write_reg(lora, REG_MODEM_CONFIG1, config);
}

void lora_set_bandwith(lora_t *lora, uint32_t bw) {
  lora->bandwidth = bw;
  uint8_t bw_index = 0;
  if (bw <= 7.8E3)
    bw_index = 0;
  else if (bw <= 10.4E3)
    bw_index = 1;
  else if (bw <= 15.6E3)
    bw_index = 2;
  else if (bw <= 20.8E3)
    bw_index = 3;
  else if (bw <= 31.25E3)
    bw_index = 4;
  else if (bw <= 41.7E3)
    bw_index = 5;
  else if (bw <= 62.5E3)
    bw_index = 6;
  else if (bw <= 125E3)
    bw_index = 7;
  else if (bw <= 250E3)
    bw_index = 8;

  uint8_t config = lora_read_reg(lora, REG_MODEM_CONFIG1);
  config = (config & 0x0F) | (bw_index << 4);

  lora_write_reg(lora, REG_MODEM_CONFIG1, config);
}

void lora_enable_crc(lora_t *lora, bool enable) {
  lora->crc_enabled = enable;
  uint8_t config = lora_read_reg(lora, REG_MODEM_CONFIG2);
  if (enable)
    config |= 0x04;
  else
    config &= ~0x04;
  lora_write_reg(lora, REG_MODEM_CONFIG2, config);
}

void lora_set_tx_power(lora_t *lora, int power) {
  uint8_t pa_select, max_power, output_power, pa_dac;

  if (power <= 14) {
    pa_select = 0;
    if (power < -4)
      power = -4;
    if (power > 14)
      power = 14;

    max_power = 0x04;

    output_power = (uint8_t)(power - (10.8 + 0.6 * max_power) + 15);
    if (output_power > 0x0F)
      output_power = 0x0F;

    pa_dac = 0x00;
  } else if (power <= 17) {
    pa_select = 1;
    if (power < 2)
      power = 2;

    max_power = 0x04;
    output_power = (uint8_t)(power - 2);
    if (output_power > 0x0F)
      output_power = 0x0F;
    pa_dac = 0x84;
  } else {
    pa_select = 1;
    if (power > 20)
      power = 20;

    max_power = 0x04;
    output_power = (uint8_t)(power - 2);
    if (output_power > 0x0F)
      output_power = 0x0F;
    pa_dac = 0x87;
  }

  uint8_t reg_val = (pa_select << 7) | (max_power << 4) | output_power;
  lora_write_reg(lora, REG_PA_CONFIG, reg_val);
  lora_write_reg(lora, REG_PA_DAC, pa_dac);

  // Not used for now, useful for logging
  /*
  double p_max = 10.8 + 0.6 * max_power;
  double p_out = power - p_max;

  if (pa_select == 0)
    p_out = p_max - (15 - p_out);
  else
    p_out = 17 - (15 - output_power);

  if (pa_dac == 0x87)
    p_out += 3.0;
  */

  lora->tx_power = power;
}

long long get_time_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool lora_send(lora_t *lora, const char *data, uint8_t len,
               uint16_t timeout_ms) {
  lora_write_reg(lora, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
  usleep(5000);

  lora_write_reg(lora, REG_FIFO_TX_BASE_ADDR, 0);
  lora_write_reg(lora, REG_FIFO_ADDR_PTR, 0);

  // Write all data bytes to FIFO
  for (uint8_t i = 0; i < len; i++) {
    lora_write_reg(lora, REG_FIFO, (uint8_t)data[i]);
  }
  lora_write_reg(lora, REG_PAYLOAD_LENGTH, len);
  lora_write_reg(lora, REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

  lora_write_reg(lora, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
  usleep(5000);

  long long start = get_time_ms();
  while (true) {
    uint8_t irq = lora_read_reg(lora, REG_IRQ_FLAGS);

    if (irq & IRQ_TX_DONE_MASK) {
      lora_write_reg(lora, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
      usleep(5000);

      lora_write_reg(lora, REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
      return true;
    }

    if (get_time_ms() - start > timeout_ms) {
      lora_write_reg(lora, REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
      usleep(5000);
      return false;
    }

    usleep(1000);
  }
}
