// Basic build test: opens the LoRa radio and sends a counter packet
#include "config.h"
#include "lora.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SEND_INTERVAL_US 1000000

int main(void) {
  lora_t *lora = lora_open(CONF_LORA_SPI_DEV, CONF_LORA_SPI_HZ, CONF_GPIO_CHIP,
                           CONF_LORA_RESET_PIN, CONF_LORA_DIO0_PIN);
  if (!lora) {
    printf("Failed to open LoRa\n");
    return 1;
  }

  lora_set_frequency(lora, CONF_LORA_FREQUENCY);
  lora_set_bandwidth(lora, CONF_LORA_BANDWIDTH);
  lora_set_spreading_factor(lora, CONF_LORA_SF);
  lora_set_coding_rate(lora, CONF_LORA_CR);
  lora_enable_crc(lora, CONF_LORA_CRC);
  lora_set_tx_power(lora, CONF_LORA_TX_POWER);
  lora_set_preamble(lora, CONF_LORA_PREAMBLE);
  lora_set_ldro(lora, CONF_LORA_LDRO);
  printf("LoRa initialized\n");

  for (uint32_t seq = 0;; seq++) {
    uint8_t packet[sizeof(seq)];
    memcpy(packet, &seq, sizeof(seq));
    bool sent = lora_send(lora, packet, sizeof(packet), CONF_LORA_TX_TIMEOUT_MS);
    printf("Sent %u: %s\n", seq, sent ? "OK" : "FAIL");
    usleep(SEND_INTERVAL_US);
  }

  lora_close(lora);
  return 0;
}
