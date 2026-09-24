#pragma once

#include "hardware/uart.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uart_inst_t *inst;
  uint32_t tx;
  uint32_t rx;
  uint32_t baud;
} uart_t;

uart_t *uart_open(uart_inst_t *inst, uint32_t tx, uint32_t rx, uint32_t baud);
void uart_close(uart_t *uart);
void uart_set_baud(uart_t *uart, uint32_t baud);
size_t uart_send(uart_t *uart, const uint8_t *buf, size_t len);
size_t uart_read(uart_t *uart, uint8_t *buf, size_t len);
