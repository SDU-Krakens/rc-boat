#include "lora.h"
#include <stdlib.h>

int main(void) {
  lora_t *lora = lora_init("/dev/spidev0.0", 1000000);
  if (lora == NULL) {
    return 1;
  }

  lora_set_frequency(lora, 433E6);
  lora_set_bandwith(lora, 125E3);
  lora_set_spreading_factor(lora, 12);
  lora_set_coding_rate(lora, 8);
  lora_enable_crc(lora, true);
  lora_set_tx_power(lora, 17);

  for (;;) {
    lora_send(lora, "Hello, world!", 14, 1000);
  }

  return 0;
}
