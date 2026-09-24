#include "uart.h"
#include "config.h"

#include "hardware/gpio.h"
#include <stdio.h>
#include <stdlib.h>

uart_t *uart_open(uart_inst_t *inst, uint32_t tx, uint32_t rx, uint32_t baud) {
  uart_t *uart = malloc(sizeof(uart_t));
  if (!uart) {
    return NULL;
  }

  uart->inst = inst;
  uart->tx = tx;
  uart->rx = rx;
  uart->baud = uart_init(inst, baud);

  gpio_set_function(tx, GPIO_FUNC_UART);
  gpio_set_function(rx, GPIO_FUNC_UART);

#ifdef CONF_DEBUG
  printf("uart_open: uart%d tx %lu rx %lu baud %lu\n", uart_get_index(inst),
         (unsigned long)tx, (unsigned long)rx, (unsigned long)uart->baud);
#endif

  return uart;
}

void uart_close(uart_t *uart) {
  if (!uart) {
    return;
  }
  uart_deinit(uart->inst);
  gpio_set_function(uart->tx, GPIO_FUNC_NULL);
  gpio_set_function(uart->rx, GPIO_FUNC_NULL);
  free(uart);
}

void uart_set_baud(uart_t *uart, uint32_t baud) {
  uart_tx_wait_blocking(uart->inst);
  uart->baud = uart_set_baudrate(uart->inst, baud);
}

size_t uart_send(uart_t *uart, const uint8_t *buf, size_t len) {
  uart_write_blocking(uart->inst, buf, len);
  return len;
}

size_t uart_read(uart_t *uart, uint8_t *buf, size_t len) {
  size_t n = 0;
  while (n < len && uart_is_readable(uart->inst)) {
    buf[n++] = (uint8_t)uart_getc(uart->inst);
  }
  return n;
}
