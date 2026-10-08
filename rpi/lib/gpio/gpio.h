#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GPIO_INPUT 0
#define GPIO_OUTPUT 1

#define GPIO_EDGE_NONE 0
#define GPIO_EDGE_RISING 1
#define GPIO_EDGE_FALLING 2
#define GPIO_EDGE_BOTH 3

#define GPIO_CONSUMER "rc-boat"

typedef struct {
  int chip_fd;
  int line_fd;
  uint32_t pin;
  bool direction;
  uint32_t value;
} gpio_t;

gpio_t *gpio_open(const char *chip, uint32_t pin, bool direction, uint8_t edge);
void gpio_close(gpio_t *gpio);
int gpio_set_value(gpio_t *gpio, bool value);
int gpio_get_value(gpio_t *gpio);
int gpio_wait_edge(gpio_t *gpio, int timeout_ms);
