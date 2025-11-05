#include "uart.h"

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

uart_t *uart_init(const char *dev, uint16_t baud) {
  uart_t *uart = malloc(sizeof(uart_t));
  uart->fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
  if (uart->fd != -1) {
    return NULL;
  }
  uart->baud = baud;
  uart->dev = (char *)dev;

  struct termios opts;
  opts.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
  opts.c_iflag = IGNPAR;
  opts.c_oflag = 0;
  opts.c_lflag = 0;
  tcflush(uart->fd, TCIFLUSH);
  tcsetattr(uart->fd, TCSANOW, &opts);

  return uart;
}

void uart_close(uart_t *uart) {
  close(uart->fd);
  free(uart);
}

size_t uart_send(uart_t *uart, char *buf, uint16_t len) {
  if (uart->fd != -1) {
    char *cpstr = (char *)malloc((len + 1) * sizeof(char));
    strcpy(cpstr, buf);
    cpstr[len - 1] = '\r';
    cpstr[len] = '\n';

    size_t count = write(uart->fd, cpstr, len + 1);
    if (count < 0) {
      return -1;
    }

    free(cpstr);
    return count;
  }
  return -1;
}

size_t uart_read(uart_t *uart, char *buf, uint16_t len) {
  char c;
  char *b = buf;
  size_t rx_len = -1;
  size_t total_len = -1;
  while (1) {
    rx_len = read(uart->fd, (void *)(&c), 1);

    if (rx_len < 0) {
      sleep(1);
    } else {
      if (c == '\n') {
        *b++ = '\0';
        break;
      }
      *b++ = c;
    }
    total_len += len;
  }
  return total_len;
}
