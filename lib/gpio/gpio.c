#include "gpio.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void gpio_export(gpio_t *gpio) {
  char *path;
  asprintf(&path, "%s/export", GPIO_PATH);

  gpio->fd = open(path, O_WRONLY);
  if (gpio->fd < 0) {
    free(path);
    printf("Failed to open GPIO export file\n");
    return;
  }

  char buf[8];
  sprintf(buf, "%d", gpio->pin + GPIO_OFFSET);
  write(gpio->fd, buf, strlen(buf));
  close(gpio->fd);
  free(path);

  gpio->exported = true;
}

void gpio_unexport(gpio_t *gpio) {
  char *path;
  asprintf(&path, "%s/unexport", GPIO_PATH);

  gpio->fd = open(path, O_WRONLY);
  if (gpio->fd < 0) {
    free(path);
    printf("Failed to open GPIO export file\n");
    return;
  }

  char buf[8];
  sprintf(buf, "%d", gpio->pin + GPIO_OFFSET);
  write(gpio->fd, buf, strlen(buf));
  close(gpio->fd);
  free(path);

  gpio->exported = true;
}

gpio_t *gpio_open(uint32_t pin) {
  gpio_t *gpio = malloc(sizeof(gpio_t));
  gpio->pin = pin;
  gpio->direction = GPIO_INPUT;
  gpio->value = 0;
  gpio->exported = false;

  gpio_export(gpio);

  return gpio;
}

void gpio_close(gpio_t *gpio) {
  gpio_unexport(gpio);
  close(gpio->fd);
  free(gpio);
}

void gpio_set_direction(gpio_t *gpio, bool direction) {
  gpio->direction = direction;

  char *path;
  asprintf(&path, "%s/gpio%d/direction", GPIO_PATH, gpio->pin + GPIO_OFFSET);

  int fd = open(path, O_WRONLY);
  if (fd < 0) {
    return;
  }

  char buf[4];

  if (direction == GPIO_INPUT)
    sprintf(buf, "in");
  else
    sprintf(buf, "out");

  write(fd, buf, strlen(buf));
  close(fd);
  free(path);
  path = NULL;
}

void gpio_set_value(gpio_t *gpio, bool value) {
  if (gpio->direction == GPIO_INPUT) {
    return;
  }

  gpio->value = value;

  char *path;
  asprintf(&path, "%s/gpio%d/value", GPIO_PATH, gpio->pin + GPIO_OFFSET);

  int fd = open(path, O_WRONLY);
  if (fd < 0) {
    return;
  }

  char buf[2];
  sprintf(buf, "%d", value);
  write(fd, buf, strlen(buf));
  close(fd);
  free(path);
  path = NULL;
}

bool gpio_get_value(gpio_t *gpio) {
  if (gpio->direction == GPIO_OUTPUT) {
    return gpio->value;
  }

  char *path;
  asprintf(&path, "%s/gpio%d/value", GPIO_PATH, gpio->pin + GPIO_OFFSET);

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    free(path);
    return false;
  }

  char buf[64];
  ssize_t bytes = read(fd, buf, 2);
  close(fd);
  free(path);

  if (bytes <= 0) {
    return false;
  }

  // Actually parse the value!
  return (buf[0] == '1');
}
