#include "i2c.h"
#include "config.h"

#include "hardware/gpio.h"
#include <stdio.h>
#include <stdlib.h>

i2c_t *i2c_open(i2c_inst_t *inst, uint32_t sda, uint32_t scl, uint32_t baud) {
  i2c_t *i2c = malloc(sizeof(i2c_t));
  if (!i2c) {
    return NULL;
  }

  i2c->inst = inst;
  i2c->sda = sda;
  i2c->scl = scl;
  i2c->baud = i2c_init(inst, baud);

  gpio_set_function(sda, GPIO_FUNC_I2C);
  gpio_set_function(scl, GPIO_FUNC_I2C);
#if CONF_I2C_PULLUPS
  gpio_pull_up(sda);
  gpio_pull_up(scl);
#endif

#ifdef CONF_DEBUG
  printf("i2c_open: i2c%d sda %lu scl %lu baud %lu\n", i2c_get_index(inst),
         (unsigned long)sda, (unsigned long)scl, (unsigned long)i2c->baud);
#endif

  return i2c;
}

void i2c_close(i2c_t *i2c) {
  if (!i2c) {
    return;
  }
  i2c_deinit(i2c->inst);
  gpio_set_function(i2c->sda, GPIO_FUNC_NULL);
  gpio_set_function(i2c->scl, GPIO_FUNC_NULL);
  free(i2c);
}

int i2c_write(i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {reg, val};
  int n = i2c_write_timeout_us(i2c->inst, addr, buf, sizeof(buf), false,
                               I2C_TIMEOUT_US);
  if (n != (int)sizeof(buf)) {
#ifdef CONF_DEBUG
    printf("i2c_write: addr %02x reg %02x failed (%d)\n", addr, reg, n);
#endif
    return -1;
  }
  return 0;
}

int i2c_read(i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
  int n = i2c_write_timeout_us(i2c->inst, addr, &reg, 1, true, I2C_TIMEOUT_US);
  if (n != 1) {
#ifdef CONF_DEBUG
    printf("i2c_read: addr %02x reg %02x select failed (%d)\n", addr, reg, n);
#endif
    return -1;
  }

  n = i2c_read_timeout_us(i2c->inst, addr, buf, len, false,
                          I2C_TIMEOUT_US * len);
  if (n != (int)len) {
#ifdef CONF_DEBUG
    printf("i2c_read: addr %02x reg %02x read failed (%d)\n", addr, reg, n);
#endif
    return -1;
  }
  return 0;
}
