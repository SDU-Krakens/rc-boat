#pragma once

#include "../gpio/gpio.h"
#include "../spi/spi.h"
#include <stdbool.h>
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
  spi_t *spi;
  uint32_t frequency;
  uint32_t bandwidth;
  uint16_t spreading_factor;
  uint16_t coding_rate;
  bool crc_enabled;
  int tx_power;
  gpio_t *reset;
} lora_t;

// Setup & initialization
lora_t *lora_init(const char *spi_dev, uint32_t spi_speed_hz,
                  uint8_t reset_pin);
void lora_end(lora_t *lora);

// Configuration (call before begin)
void lora_set_frequency(lora_t *lora, uint32_t frequency);
void lora_set_spreading_factor(lora_t *lora, uint16_t sf);
void lora_set_coding_rate(lora_t *lora, uint16_t cr);
void lora_set_bandwith(lora_t *lora, uint32_t bw);
void lora_enable_crc(lora_t *lora, bool enable);
void lora_set_tx_power(lora_t *lora, int power);

bool lora_send(lora_t *lora, const char *data, uint8_t len,
               uint16_t timeout_ms);
