#include "log.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <errno.h>

#define LOG_NAME_FMT "%Y-%m-%dT%H-%M-%SZ"
#define LOG_NAME_MAX 32
#define LOG_EXT ".log"
#define LOG_SUFFIX_MAX 99
#define LOG_TIME_FMT "%Y-%m-%dT%H:%M:%S"
#define NS_PER_MS 1000000L

static const char *const src_names[LOG_SRC_COUNT] = {
    [LOG_SRC_PICO_MAIN] = "pico", [LOG_SRC_I2C] = "i2c",
    [LOG_SRC_UART] = "uart",      [LOG_SRC_IMU] = "imu",
    [LOG_SRC_GPS] = "gps",        [LOG_SRC_CAN] = "can",
    [LOG_SRC_STORAGE] = "storage", [LOG_SRC_ZERO_MAIN] = "zero",
    [LOG_SRC_GPIO] = "gpio",      [LOG_SRC_SPI] = "spi",
    [LOG_SRC_LORA] = "lora",
};

log_t *log_open(const char *dir) {
  log_t *log = calloc(1, sizeof(log_t));
  if (!log) {
    return NULL;
  }

  time_t now = time(NULL);
  struct tm utc;
  char name[LOG_NAME_MAX];
  gmtime_r(&now, &utc);
  strftime(name, sizeof(name), LOG_NAME_FMT, &utc);

  // "wx" fails if the file exists, then try name-1, name-2, ...
  for (int i = 0; i <= LOG_SUFFIX_MAX; i++) {
    if (i == 0) {
      snprintf(log->path, sizeof(log->path), "%s/%s" LOG_EXT, dir, name);
    } else {
      snprintf(log->path, sizeof(log->path), "%s/%s-%d" LOG_EXT, dir, name, i);
    }
    log->fp = fopen(log->path, "wx");
    if (log->fp || errno != EEXIST) {
      break;
    }
  }

  if (!log->fp) {
#ifdef CONF_DEBUG
    printf("log_open: can't open %s\n", log->path);
#endif
    free(log);
    return NULL;
  }
  return log;
}

static void drop_oldest(log_t *log) {
  log_entry_t *e = log->head;
  log->head = e->next;
  if (!log->head) {
    log->tail = NULL;
  }
  log->count--;
  free(e);
}

void log_close(log_t *log) {
  if (!log) {
    return;
  }
  while (log->head) {
    drop_oldest(log);
  }
  if (log->fp) {
    fclose(log->fp);
  }
  free(log);
}

static void format_time(char *buf, size_t max) {
  struct timespec ts;
  struct tm utc;
  clock_gettime(CLOCK_REALTIME, &ts);
  gmtime_r(&ts.tv_sec, &utc);
  size_t n = strftime(buf, max, LOG_TIME_FMT, &utc);
  snprintf(buf + n, max - n, ".%03ldZ", ts.tv_nsec / NS_PER_MS);
}

static void enqueue(log_t *log, const char *line) {
  if (log->count >= CONF_LOG_LORA_QUEUE) {
    drop_oldest(log);
    log->dropped++;
  }

  size_t len = strlen(line);
  log_entry_t *e = malloc(sizeof(log_entry_t) + len + 1);
  if (!e) {
    log->dropped++;
    return;
  }
  e->next = NULL;
  memcpy(e->text, line, len + 1);

  if (log->tail) {
    log->tail->next = e;
  } else {
    log->head = e;
  }
  log->tail = e;
  log->count++;
}

int log_print(log_t *log, log_src_t src, const char *fmt, ...) {
  char stamp[LOG_TIME_MAX];
  char msg[CONF_LOG_LINE_MAX];
  char line[CONF_LOG_LINE_MAX];
  va_list ap;

  format_time(stamp, sizeof(stamp));
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  const char *name = src < LOG_SRC_COUNT ? src_names[src] : "?";
  int n = snprintf(line, sizeof(line), "%s %s %s", stamp, name, msg);
  if (n < 0) {
    return -1;
  }
  // Longer lines are cut at CONF_LOG_LINE_MAX, on purpose

  enqueue(log, line);

  if (fprintf(log->fp, "%s\n", line) < 0 || fflush(log->fp) != 0) {
    return -1;
  }
  return 0;
}

int log_lora_next(log_t *log, char *buf, size_t max) {
  if (!log->head || max == 0) {
    return 0;
  }

  size_t n = strnlen(log->head->text, max - 1);
  memcpy(buf, log->head->text, n);
  buf[n] = '\0';
  drop_oldest(log);
  return (int)n;
}
