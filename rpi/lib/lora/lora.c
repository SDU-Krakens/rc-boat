#include "lora.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Bandwidth register index -> upper bound in Hz
static const uint32_t bw_table[] = {7800,  10400, 15600,  20800,  31250,
                                    41700, 62500, 125000, 250000, 500000};
#define BW_COUNT (sizeof(bw_table) / sizeof(bw_table[0]))

// PA configuration (kept from the old driver)
#define PA_SELECT_RFO 0
#define PA_SELECT_BOOST 1
#define PA_SELECT_SHIFT 7
#define PA_MAX_POWER 0x04
#define PA_MAX_POWER_SHIFT 4
#define PA_OUTPUT_MAX 0x0F
#define PA_DAC_RFO 0x00
#define PA_DAC_DEFAULT 0x84
#define PA_DAC_HIGH_POWER 0x87
#define PA_RFO_MIN_DBM -4
#define PA_RFO_MAX_DBM 14
#define PA_BOOST_MIN_DBM 2
#define PA_BOOST_MAX_DBM 17
#define PA_HIGH_MAX_DBM 20
#define PA_BOOST_OFFSET_DBM 2
#define PA_RFO_PMAX_BASE 10.8
#define PA_RFO_PMAX_STEP 0.6
#define PA_RFO_OUTPUT_OFFSET 15

static uint8_t read_reg(lora_t *lora, uint8_t reg) {
  uint8_t tx[2] = {reg & SPI_ADDR_MASK, 0};
  uint8_t rx[2] = {0};
  spi_transfer(lora->spi, tx, rx, sizeof(tx));
#ifdef CONF_DEBUG
  printf("lora read: %02x %02x\n", reg, rx[1]);
#endif
  return rx[1];
}

static void write_reg(lora_t *lora, uint8_t reg, uint8_t val) {
  uint8_t tx[2] = {reg | SPI_WRITE_BIT, val};
  spi_transfer(lora->spi, tx, NULL, sizeof(tx));
#ifdef CONF_DEBUG
  printf("lora write: %02x %02x\n", reg, val);
#endif
}

static void set_mode(lora_t *lora, uint8_t mode) {
  write_reg(lora, REG_OP_MODE, MODE_LONG_RANGE_MODE | mode);
}

// Drop DIO0 edges left over from an earlier operation
static void flush_dio0(lora_t *lora) {
  while (gpio_wait_edge(lora->dio0, 0) > 0) {
  }
}

static void apply_ldro(lora_t *lora) {
  bool on;
  if (lora->ldro == LORA_LDRO_AUTO) {
    uint64_t symbol_ms =
        ((uint64_t)1000 << lora->spreading_factor) / lora->bandwidth;
    on = symbol_ms > LORA_LDRO_SYMBOL_MS;
  } else {
    on = lora->ldro == LORA_LDRO_ON;
  }

  uint8_t config = read_reg(lora, REG_MODEM_CONFIG3);
  if (on) {
    config |= LORA_CONFIG3_LDRO;
  } else {
    config &= ~LORA_CONFIG3_LDRO;
  }
  write_reg(lora, REG_MODEM_CONFIG3, config);
}

lora_t *lora_open(const char *spi_dev, uint32_t spi_speed_hz,
                  const char *gpio_chip, uint32_t reset_pin,
                  uint32_t dio0_pin) {
  lora_t *lora = calloc(1, sizeof(lora_t));
  if (!lora) {
    return NULL;
  }

  lora->reset = gpio_open(gpio_chip, reset_pin, GPIO_OUTPUT, GPIO_EDGE_NONE);
  lora->dio0 = gpio_open(gpio_chip, dio0_pin, GPIO_INPUT, GPIO_EDGE_RISING);
  lora->spi = spi_open(spi_dev, spi_speed_hz, LORA_SPI_MODE);
  if (!lora->reset || !lora->dio0 || !lora->spi) {
    lora_close(lora);
    return NULL;
  }

  gpio_set_value(lora->reset, 0);
  usleep(LORA_RESET_LOW_US);
  gpio_set_value(lora->reset, 1);
  usleep(LORA_RESET_WAIT_US);

  uint8_t ver = read_reg(lora, REG_VERSION);
  if (ver != LORA_VERSION_SX1278) {
#ifdef CONF_DEBUG
    printf("lora_open: wrong version %02x\n", ver);
#endif
    lora_close(lora);
    return NULL;
  }

  lora->frequency = LORA_DEFAULT_FREQUENCY;
  lora->bandwidth = LORA_DEFAULT_BANDWIDTH;
  lora->spreading_factor = LORA_DEFAULT_SF;
  lora->coding_rate = LORA_DEFAULT_CR;
  lora->crc_enabled = true;
  lora->tx_power = LORA_DEFAULT_TX_POWER;
  lora->preamble = LORA_DEFAULT_PREAMBLE;
  lora->ldro = LORA_LDRO_AUTO;

  set_mode(lora, MODE_SLEEP);
  usleep(LORA_MODE_SWITCH_US);
  set_mode(lora, MODE_STDBY);
  usleep(LORA_MODE_SWITCH_US);

  lora_set_preamble(lora, lora->preamble);
  write_reg(lora, REG_FIFO_TX_BASE_ADDR, LORA_FIFO_TX_BASE);
  write_reg(lora, REG_FIFO_RX_BASE_ADDR, LORA_FIFO_RX_BASE);
  write_reg(lora, REG_LNA, LORA_LNA_MAX_GAIN_BOOST);
  write_reg(lora, REG_MODEM_CONFIG3, LORA_CONFIG3_AGC_AUTO);

  return lora;
}

void lora_close(lora_t *lora) {
  if (!lora) {
    return;
  }
  if (lora->spi) {
    set_mode(lora, MODE_SLEEP);
    spi_close(lora->spi);
  }
  gpio_close(lora->reset);
  gpio_close(lora->dio0);
  free(lora);
}

void lora_set_frequency(lora_t *lora, uint32_t hz) {
  lora->frequency = hz;
  uint64_t frf = ((uint64_t)hz << LORA_FRF_SHIFT) / LORA_FXOSC;
  write_reg(lora, REG_FRF_MSB, frf >> 16);
  write_reg(lora, REG_FRF_MID, frf >> 8);
  write_reg(lora, REG_FRF_LSB, frf >> 0);
}

void lora_set_bandwidth(lora_t *lora, uint32_t hz) {
  uint8_t idx = BW_COUNT - 1;
  for (uint8_t i = 0; i < BW_COUNT; i++) {
    if (hz <= bw_table[i]) {
      idx = i;
      break;
    }
  }
  lora->bandwidth = bw_table[idx];

  uint8_t config = read_reg(lora, REG_MODEM_CONFIG1);
  config = (config & LORA_LOW_NIBBLE) | (idx << LORA_CONFIG1_BW_SHIFT);
  write_reg(lora, REG_MODEM_CONFIG1, config);
  apply_ldro(lora);
}

void lora_set_spreading_factor(lora_t *lora, uint16_t sf) {
  if (sf < LORA_SF_MIN) {
    sf = LORA_SF_MIN;
  }
  if (sf > LORA_SF_MAX) {
    sf = LORA_SF_MAX;
  }
  lora->spreading_factor = sf;

  uint8_t config = read_reg(lora, REG_MODEM_CONFIG2);
  config = (config & LORA_LOW_NIBBLE) |
           ((sf << LORA_CONFIG2_SF_SHIFT) & LORA_HIGH_NIBBLE);
  write_reg(lora, REG_MODEM_CONFIG2, config);
  apply_ldro(lora);
}

void lora_set_coding_rate(lora_t *lora, uint16_t cr) {
  if (cr < LORA_CR_MIN) {
    cr = LORA_CR_MIN;
  }
  if (cr > LORA_CR_MAX) {
    cr = LORA_CR_MAX;
  }
  lora->coding_rate = cr;

  uint8_t config = read_reg(lora, REG_MODEM_CONFIG1);
  config = (config & LORA_CONFIG1_CR_MASK) |
           ((cr - LORA_CR_OFFSET) << LORA_CONFIG1_CR_SHIFT);
  write_reg(lora, REG_MODEM_CONFIG1, config);
}

void lora_enable_crc(lora_t *lora, bool enable) {
  lora->crc_enabled = enable;
  uint8_t config = read_reg(lora, REG_MODEM_CONFIG2);
  if (enable) {
    config |= LORA_CONFIG2_CRC_ON;
  } else {
    config &= ~LORA_CONFIG2_CRC_ON;
  }
  write_reg(lora, REG_MODEM_CONFIG2, config);
}

void lora_set_tx_power(lora_t *lora, int dbm) {
  uint8_t pa_select, output_power, pa_dac;

  if (dbm <= PA_RFO_MAX_DBM) {
    pa_select = PA_SELECT_RFO;
    if (dbm < PA_RFO_MIN_DBM) {
      dbm = PA_RFO_MIN_DBM;
    }
    output_power = (uint8_t)(dbm - (PA_RFO_PMAX_BASE +
                                    PA_RFO_PMAX_STEP * PA_MAX_POWER) +
                             PA_RFO_OUTPUT_OFFSET);
    pa_dac = PA_DAC_RFO;
  } else if (dbm <= PA_BOOST_MAX_DBM) {
    pa_select = PA_SELECT_BOOST;
    output_power = (uint8_t)(dbm - PA_BOOST_OFFSET_DBM);
    pa_dac = PA_DAC_DEFAULT;
  } else {
    pa_select = PA_SELECT_BOOST;
    if (dbm > PA_HIGH_MAX_DBM) {
      dbm = PA_HIGH_MAX_DBM;
    }
    output_power = (uint8_t)(dbm - PA_BOOST_OFFSET_DBM);
    pa_dac = PA_DAC_HIGH_POWER;
  }
  if (output_power > PA_OUTPUT_MAX) {
    output_power = PA_OUTPUT_MAX;
  }

  write_reg(lora, REG_PA_CONFIG,
            (pa_select << PA_SELECT_SHIFT) |
                (PA_MAX_POWER << PA_MAX_POWER_SHIFT) | output_power);
  write_reg(lora, REG_PA_DAC, pa_dac);
  lora->tx_power = dbm;
}

void lora_set_preamble(lora_t *lora, uint16_t len) {
  lora->preamble = len;
  write_reg(lora, REG_PREAMBLE_MSB, len >> 8);
  write_reg(lora, REG_PREAMBLE_LSB, len & 0xFF);
}

void lora_set_ldro(lora_t *lora, int mode) {
  lora->ldro = mode;
  apply_ldro(lora);
}

bool lora_send(lora_t *lora, const uint8_t *data, uint8_t len,
               uint16_t timeout_ms) {
  set_mode(lora, MODE_STDBY);
  write_reg(lora, REG_DIO_MAPPING1, DIO0_TX_DONE);
  write_reg(lora, REG_FIFO_TX_BASE_ADDR, LORA_FIFO_TX_BASE);
  write_reg(lora, REG_FIFO_ADDR_PTR, LORA_FIFO_TX_BASE);

  // Whole payload in one transfer, the FIFO pointer auto-increments
  uint8_t tx[1 + LORA_FIFO_SIZE];
  tx[0] = REG_FIFO | SPI_WRITE_BIT;
  memcpy(&tx[1], data, len);
  spi_transfer(lora->spi, tx, NULL, 1 + (size_t)len);

  write_reg(lora, REG_PAYLOAD_LENGTH, len);
  write_reg(lora, REG_IRQ_FLAGS, IRQ_ALL_MASK);
  flush_dio0(lora);

  set_mode(lora, MODE_TX);
  int rc = gpio_wait_edge(lora->dio0, timeout_ms);
  bool done = rc > 0 && (read_reg(lora, REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK);

  write_reg(lora, REG_IRQ_FLAGS, IRQ_ALL_MASK);
  set_mode(lora, MODE_STDBY);
  return done;
}

int lora_receive_mode(lora_t *lora, bool continuous) {
  set_mode(lora, MODE_STDBY);
  write_reg(lora, REG_DIO_MAPPING1, DIO0_RX_DONE);
  write_reg(lora, REG_FIFO_RX_BASE_ADDR, LORA_FIFO_RX_BASE);
  write_reg(lora, REG_FIFO_ADDR_PTR, LORA_FIFO_RX_BASE);
  write_reg(lora, REG_IRQ_FLAGS, IRQ_ALL_MASK);
  flush_dio0(lora);
  set_mode(lora, continuous ? MODE_RX_CONTINUOUS : MODE_RX_SINGLE);
  return 0;
}

int lora_receive(lora_t *lora, uint8_t *buf, uint8_t max, int timeout_ms) {
  uint8_t irq = read_reg(lora, REG_IRQ_FLAGS);
  if (!(irq & (IRQ_RX_DONE_MASK | IRQ_RX_TIMEOUT_MASK))) {
    int rc = gpio_wait_edge(lora->dio0, timeout_ms);
    if (rc <= 0) {
      return rc; // 0 timeout, -1 error
    }
    irq = read_reg(lora, REG_IRQ_FLAGS);
  }

  if (!(irq & IRQ_RX_DONE_MASK)) {
    write_reg(lora, REG_IRQ_FLAGS, IRQ_ALL_MASK);
    return 0; // single mode timed out on the chip
  }
  if (irq & IRQ_PAYLOAD_CRC_ERROR_MASK) {
    write_reg(lora, REG_IRQ_FLAGS, IRQ_ALL_MASK);
    return -1;
  }

  uint8_t nb = read_reg(lora, REG_RX_NB_BYTES);
  uint8_t n = nb < max ? nb : max;
  write_reg(lora, REG_FIFO_ADDR_PTR, read_reg(lora, REG_FIFO_RX_CURRENT_ADDR));

  uint8_t tx[1 + LORA_FIFO_SIZE] = {REG_FIFO & SPI_ADDR_MASK};
  uint8_t rx[1 + LORA_FIFO_SIZE];
  spi_transfer(lora->spi, tx, rx, 1 + (size_t)n);
  memcpy(buf, &rx[1], n);

  write_reg(lora, REG_IRQ_FLAGS, IRQ_ALL_MASK);
  return n;
}

int lora_packet_rssi(lora_t *lora) {
  int offset = lora->frequency < LORA_LF_MAX_HZ ? LORA_RSSI_OFFSET_LF
                                                : LORA_RSSI_OFFSET_HF;
  return offset + read_reg(lora, REG_PKT_RSSI_VALUE);
}

float lora_packet_snr(lora_t *lora) {
  return (int8_t)read_reg(lora, REG_PKT_SNR_VALUE) / LORA_SNR_DIV;
}
