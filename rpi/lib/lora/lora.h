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
#define REG_LNA 0x0C
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_FIFO_TX_BASE_ADDR 0x0E
#define REG_FIFO_RX_BASE_ADDR 0x0F
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI_VALUE 0x1A
#define REG_MODEM_CONFIG1 0x1D
#define REG_MODEM_CONFIG2 0x1E
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG3 0x26
#define REG_DIO_MAPPING1 0x40
#define REG_VERSION 0x42
#define REG_PREAMBLE_MSB 0x43
#define REG_PREAMBLE_LSB 0x44
#define REG_PA_DAC 0x4D

// --- Mode bits ---
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05
#define MODE_RX_SINGLE 0x06

// --- IRQ flags ---
#define IRQ_RX_TIMEOUT_MASK 0x80
#define IRQ_RX_DONE_MASK 0x40
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_TX_DONE_MASK 0x08
#define IRQ_ALL_MASK 0xFF

// --- DIO0 mapping (REG_DIO_MAPPING1 bits 7:6) ---
#define DIO0_RX_DONE 0x00
#define DIO0_TX_DONE 0x40

// --- Register fields ---
#define SPI_WRITE_BIT 0x80
#define SPI_ADDR_MASK 0x7F
#define LORA_VERSION_SX1278 0x12
#define LORA_FIFO_SIZE 256
#define LORA_FIFO_TX_BASE 0x00
#define LORA_FIFO_RX_BASE 0x00
#define LORA_LNA_MAX_GAIN_BOOST 0x23
#define LORA_CONFIG3_AGC_AUTO 0x04
#define LORA_CONFIG3_LDRO 0x08
#define LORA_CONFIG2_CRC_ON 0x04
#define LORA_CONFIG2_SF_SHIFT 4
#define LORA_CONFIG1_BW_SHIFT 4
#define LORA_CONFIG1_CR_SHIFT 1
#define LORA_CONFIG1_CR_MASK 0xF1
#define LORA_LOW_NIBBLE 0x0F
#define LORA_HIGH_NIBBLE 0xF0
#define LORA_SF_MIN 6
#define LORA_SF_MAX 12
#define LORA_CR_MIN 5
#define LORA_CR_MAX 8
#define LORA_CR_OFFSET 4

// Frequency synthesiser: Frf = f * 2^19 / Fxosc
#define LORA_FXOSC 32000000
#define LORA_FRF_SHIFT 19

// Low data rate optimisation is required above this symbol time
#define LORA_LDRO_SYMBOL_MS 16

// Packet RSSI offsets (datasheet 5.5.5), LF port below 779 MHz
#define LORA_RSSI_OFFSET_LF (-164)
#define LORA_RSSI_OFFSET_HF (-157)
#define LORA_LF_MAX_HZ 779000000
#define LORA_SNR_DIV 4.0f

// SPI mode for the SX1278
#define LORA_SPI_MODE 0

// Reset pulse timing
#define LORA_RESET_LOW_US 10000
#define LORA_RESET_WAIT_US 10000
#define LORA_MODE_SWITCH_US 10000

// Defaults stored at open (applied by the setters)
#define LORA_DEFAULT_FREQUENCY 433000000
#define LORA_DEFAULT_BANDWIDTH 125000
#define LORA_DEFAULT_SF 7
#define LORA_DEFAULT_CR 5
#define LORA_DEFAULT_TX_POWER 17
#define LORA_DEFAULT_PREAMBLE 8

#define LORA_LDRO_AUTO -1
#define LORA_LDRO_OFF 0
#define LORA_LDRO_ON 1

typedef struct {
  spi_t *spi;
  gpio_t *reset;
  gpio_t *dio0;
  uint32_t frequency, bandwidth;
  uint16_t spreading_factor, coding_rate;
  bool crc_enabled;
  int tx_power;
  uint16_t preamble;
  int ldro; // LORA_LDRO_*
} lora_t;

lora_t *lora_open(const char *spi_dev, uint32_t spi_speed_hz,
                  const char *gpio_chip, uint32_t reset_pin, uint32_t dio0_pin);
void lora_close(lora_t *lora);
void lora_set_frequency(lora_t *lora, uint32_t hz);
void lora_set_bandwidth(lora_t *lora, uint32_t hz);
void lora_set_spreading_factor(lora_t *lora, uint16_t sf);
void lora_set_coding_rate(lora_t *lora, uint16_t cr);
void lora_enable_crc(lora_t *lora, bool enable);
void lora_set_tx_power(lora_t *lora, int dbm);
void lora_set_preamble(lora_t *lora, uint16_t len);
void lora_set_ldro(lora_t *lora, int mode);
bool lora_send(lora_t *lora, const uint8_t *data, uint8_t len,
               uint16_t timeout_ms);
int lora_receive_mode(lora_t *lora, bool continuous);
int lora_receive(lora_t *lora, uint8_t *buf, uint8_t max, int timeout_ms);
int lora_packet_rssi(lora_t *lora);
float lora_packet_snr(lora_t *lora);
