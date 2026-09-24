#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  char *dev;
  int fd;
  uint32_t baud;
} uart_t;

uart_t *uart_open(const char *dev, uint32_t baud);
void uart_close(uart_t *uart);
size_t uart_send(uart_t *uart, const uint8_t *src, size_t len);
size_t uart_read(uart_t *uart, uint8_t *dest, size_t max);
