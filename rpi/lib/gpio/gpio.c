#include "gpio.h"
#include "config.h"

#include <fcntl.h>
#include <linux/gpio.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define GPIO_LINE_MASK 1u // single line per request, index 0

gpio_t *gpio_open(const char *chip, uint32_t pin, bool direction,
                  uint8_t edge) {
  gpio_t *gpio = malloc(sizeof(gpio_t));
  if (!gpio) {
    return NULL;
  }
  gpio->pin = pin;
  gpio->direction = direction;
  gpio->value = 0;
  gpio->line_fd = -1;

  gpio->chip_fd = open(chip, O_RDWR | O_CLOEXEC);
  if (gpio->chip_fd < 0) {
#ifdef CONF_DEBUG
    printf("gpio_open: can't open %s\n", chip);
#endif
    free(gpio);
    return NULL;
  }

  struct gpio_v2_line_request req;
  memset(&req, 0, sizeof(req));
  req.offsets[0] = pin;
  req.num_lines = 1;
  strncpy(req.consumer, GPIO_CONSUMER, sizeof(req.consumer) - 1);

  if (direction == GPIO_OUTPUT) {
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    // Start low
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    req.config.attrs[0].attr.values = 0;
    req.config.attrs[0].mask = GPIO_LINE_MASK;
  } else {
    req.config.flags = GPIO_V2_LINE_FLAG_INPUT;
    if (edge & GPIO_EDGE_RISING) {
      req.config.flags |= GPIO_V2_LINE_FLAG_EDGE_RISING;
    }
    if (edge & GPIO_EDGE_FALLING) {
      req.config.flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING;
    }
  }

  if (ioctl(gpio->chip_fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
#ifdef CONF_DEBUG
    printf("gpio_open: can't request line %u\n", pin);
#endif
    close(gpio->chip_fd);
    free(gpio);
    return NULL;
  }
  gpio->line_fd = req.fd;

#ifdef CONF_DEBUG
  printf("gpio_open: line %u %s\n", pin,
         direction == GPIO_OUTPUT ? "output" : "input");
#endif
  return gpio;
}

void gpio_close(gpio_t *gpio) {
  if (!gpio) {
    return;
  }
  if (gpio->line_fd >= 0) {
    close(gpio->line_fd);
  }
  if (gpio->chip_fd >= 0) {
    close(gpio->chip_fd);
  }
  free(gpio);
}

int gpio_set_value(gpio_t *gpio, bool value) {
  if (gpio->direction != GPIO_OUTPUT) {
    return -1;
  }

  struct gpio_v2_line_values vals = {
      .bits = value ? GPIO_LINE_MASK : 0,
      .mask = GPIO_LINE_MASK,
  };
  if (ioctl(gpio->line_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals) < 0) {
    return -1;
  }
  gpio->value = value;
  return 0;
}

int gpio_get_value(gpio_t *gpio) {
  struct gpio_v2_line_values vals = {.bits = 0, .mask = GPIO_LINE_MASK};
  if (ioctl(gpio->line_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &vals) < 0) {
    return -1;
  }
  return (vals.bits & GPIO_LINE_MASK) ? 1 : 0;
}

int gpio_wait_edge(gpio_t *gpio, int timeout_ms) {
  struct pollfd pfd = {.fd = gpio->line_fd, .events = POLLIN};

  int rc = poll(&pfd, 1, timeout_ms);
  if (rc <= 0) {
    return rc; // 0 timeout, -1 error
  }

  struct gpio_v2_line_event event;
  if (read(gpio->line_fd, &event, sizeof(event)) != sizeof(event)) {
    return -1;
  }
  return 1;
}
