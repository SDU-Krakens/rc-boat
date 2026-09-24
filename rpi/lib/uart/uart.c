#include "uart.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// Wait for the port to accept more bytes after a partial write
#define UART_WRITE_WAIT_MS 100

static speed_t baud_to_speed(uint32_t baud) {
  switch (baud) {
  case 9600: return B9600;
  case 19200: return B19200;
  case 38400: return B38400;
  case 57600: return B57600;
  case 115200: return B115200;
  case 230400: return B230400;
  case 460800: return B460800;
  case 921600: return B921600;
  default: return 0;
  }
}

uart_t *uart_open(const char *dev, uint32_t baud) {
  speed_t speed = baud_to_speed(baud);
  if (!speed) {
    log_err(LOG_SRC_UART, "unsupported baud %u", baud);
    return NULL;
  }

  uart_t *uart = malloc(sizeof(uart_t));
  if (!uart) {
    return NULL;
  }

  uart->fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (uart->fd < 0) {
    log_err(LOG_SRC_UART, "can't open %s: %s", dev, strerror(errno));
    free(uart);
    return NULL;
  }
  uart->dev = strdup(dev);
  uart->baud = baud;

  // Raw 8N1, no flow control
  struct termios tio;
  memset(&tio, 0, sizeof(tio));
  cfmakeraw(&tio);
  tio.c_cflag |= CLOCAL | CREAD;
  tio.c_cflag &= ~(CSTOPB | PARENB | CRTSCTS);
  cfsetispeed(&tio, speed);
  cfsetospeed(&tio, speed);
  if (tcsetattr(uart->fd, TCSANOW, &tio) != 0) {
    log_err(LOG_SRC_UART, "can't configure %s: %s", dev, strerror(errno));
    uart_close(uart);
    return NULL;
  }
  tcflush(uart->fd, TCIOFLUSH);

  log_info(LOG_SRC_UART, "opened %s at %u baud", dev, baud);
  return uart;
}

void uart_close(uart_t *uart) {
  if (!uart) {
    return;
  }
  close(uart->fd);
  free(uart->dev);
  free(uart);
}

size_t uart_send(uart_t *uart, const uint8_t *src, size_t len) {
  size_t done = 0;

  while (done < len) {
    ssize_t n = write(uart->fd, src + done, len - done);
    if (n > 0) {
      done += n;
      continue;
    }
    if (n < 0 && errno != EAGAIN && errno != EINTR) {
      log_err(LOG_SRC_UART, "write failed: %s", strerror(errno));
      break;
    }
    // Port buffer full, wait until it can take more
    struct pollfd pfd = {.fd = uart->fd, .events = POLLOUT};
    poll(&pfd, 1, UART_WRITE_WAIT_MS);
  }
  return done;
}

size_t uart_read(uart_t *uart, uint8_t *dest, size_t max) {
  ssize_t n = read(uart->fd, dest, max);
  return n > 0 ? (size_t)n : 0;
}
