#pragma once

#include "../spi/spi.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  spi_t *spi;
  uint32_t frequency;
  uint32_t bandwidth;
  uint16_t spreading_factor;
  uint16_t coding_rate;
  bool crc_enabled;
  int tx_power;
  uint8_t reset_pin;
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
