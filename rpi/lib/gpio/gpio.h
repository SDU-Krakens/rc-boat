#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef GPIO_OFFSET
#define GPIO_OFFSET 512
#endif

#ifndef GPIO_PATH
#define GPIO_PATH "/sys/class/gpio"
#endif

#define GPIO_INPUT 0
#define GPIO_OUTPUT 1

typedef struct {
  bool direction;
  bool exported;
  uint32_t value;
  uint32_t pin;
  int fd;
} gpio_t;

gpio_t *gpio_open(uint32_t pin);
void gpio_close(gpio_t *gpio);
void gpio_set_direction(gpio_t *gpio, bool direction);
void gpio_set_value(gpio_t *gpio, bool value);
bool gpio_get_value(gpio_t *gpio);
