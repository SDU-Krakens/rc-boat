#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  char *dev;
  uint32_t baud;
  int fd;
} uart_t;

uart_t *uart_init(const char *dev, uint32_t baud);
void uart_close(uart_t *uart);
size_t uart_send(uart_t *uart, char *buf, size_t len);
size_t uart_read(uart_t *uart, char *buf, size_t len);
