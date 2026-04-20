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

uart_t *uart_init(const char *dev, uint32_t baud) {
  uart_t *uart = malloc(sizeof(uart_t));
  uart->fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
  if (uart->fd == -1) {
    free(uart);
    return NULL;
  }
  uart->baud = baud;
  uart->dev = strdup(dev);

  speed_t speed;
  switch (baud) {
  case 4800:   speed = B4800;   break;
  case 9600:   speed = B9600;   break;
  case 19200:  speed = B19200;  break;
  case 38400:  speed = B38400;  break;
  case 57600:  speed = B57600;  break;
  case 115200: speed = B115200; break;
  default:     speed = B9600;   break;
  }

  struct termios opts;
  memset(&opts, 0, sizeof(opts));
  opts.c_cflag = speed | CS8 | CLOCAL | CREAD;
  opts.c_iflag = IGNPAR;
  opts.c_oflag = 0;
  opts.c_lflag = 0;
  tcflush(uart->fd, TCIFLUSH);
  tcsetattr(uart->fd, TCSANOW, &opts);

  return uart;
}

void uart_close(uart_t *uart) {
  close(uart->fd);
  free(uart->dev);
  free(uart);
}

size_t uart_send(uart_t *uart, char *buf, size_t len) {
  if (uart->fd != -1) {
    char *cpstr = (char *)malloc((len + 1) * sizeof(char));
    strcpy(cpstr, buf);
    cpstr[len - 1] = '\r';
    cpstr[len] = '\n';

    size_t count = write(uart->fd, cpstr, len + 1);

    free(cpstr);
    return count;
  }
  return -1;
}

size_t uart_read(uart_t *uart, char *buf, size_t len) {
  char c;
  char *b = buf;
  size_t total_len = 0;

  while (total_len < len - 1) { // Leave room for null terminator
    ssize_t rx_len = read(uart->fd, (void *)(&c), 1);

    if (rx_len <= 0) {
      usleep(1000); // Wait a bit if no data
      continue;
    }

    if (c == '\n') {
      *b = '\0';
      break;
    }

    *b++ = c;
    total_len++;
  }

  *b = '\0'; // Ensure null termination
  return total_len;
}
