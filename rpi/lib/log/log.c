#include "log.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_NAME_FMT "%Y-%m-%dT%H-%M-%SZ"
#define LOG_NAME_MAX 32
#define LOG_EXT ".log"
#define LOG_SUFFIX_MAX 99
#define LOG_TIME_FMT "%Y-%m-%dT%H:%M:%S"
#define NS_PER_MS 1000000L
#define MS_PER_S 1000L

typedef struct log_entry {
  struct log_entry *next;
  char text[];
} log_entry_t;

typedef struct {
  log_entry_t *head, *tail; // oldest first
  uint32_t count;
} log_list_t;

static FILE *log_fp;
static char log_path[CONF_LOG_PATH_MAX];
static log_list_t lora_queue;  // lines waiting for LoRa, bounded
static log_list_t pending;     // lines logged before log_open, unbounded
static uint32_t lora_dropped;
static int64_t last_flush_ms;
// Used from the reader and the LoRa thread
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static int64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * MS_PER_S + ts.tv_nsec / NS_PER_MS;
}

static bool list_push(log_list_t *list, const char *line) {
  size_t len = strlen(line);
  log_entry_t *e = malloc(sizeof(log_entry_t) + len + 1);
  if (!e) {
    return false;
  }
  e->next = NULL;
  memcpy(e->text, line, len + 1);

  if (list->tail) {
    list->tail->next = e;
  } else {
    list->head = e;
  }
  list->tail = e;
  list->count++;
  return true;
}

static void list_pop(log_list_t *list) {
  log_entry_t *e = list->head;
  list->head = e->next;
  if (!list->head) {
    list->tail = NULL;
  }
  list->count--;
  free(e);
}

// Flushed to the kernel at most every CONF_LOG_FLUSH_MS
static int write_line(const char *line) {
  if (fprintf(log_fp, "%s\n", line) < 0) {
    return -1;
  }
  int64_t now = now_ms();
  if (now - last_flush_ms >= CONF_LOG_FLUSH_MS) {
    last_flush_ms = now;
    if (fflush(log_fp) != 0) {
      return -1;
    }
  }
  return 0;
}

int log_open(const char *dir) {
  pthread_mutex_lock(&log_lock);
  time_t now = time(NULL);
  struct tm utc;
  char name[LOG_NAME_MAX];
  gmtime_r(&now, &utc);
  strftime(name, sizeof(name), LOG_NAME_FMT, &utc);

  // "wx" fails if the file exists, then try name-1, name-2, ...
  for (int i = 0; i <= LOG_SUFFIX_MAX; i++) {
    if (i == 0) {
      snprintf(log_path, sizeof(log_path), "%s/%s" LOG_EXT, dir, name);
    } else {
      snprintf(log_path, sizeof(log_path), "%s/%s-%d" LOG_EXT, dir, name, i);
    }
    log_fp = fopen(log_path, "wx");
    if (log_fp || errno != EEXIST) {
      break;
    }
  }
  if (!log_fp) {
    pthread_mutex_unlock(&log_lock);
    return -1;
  }

  // Lines logged before the file existed
  while (pending.head) {
    write_line(pending.head->text);
    list_pop(&pending);
  }
  pthread_mutex_unlock(&log_lock);
  return 0;
}

void log_close(void) {
  pthread_mutex_lock(&log_lock);
  while (lora_queue.head) {
    list_pop(&lora_queue);
  }
  if (log_fp) {
    fclose(log_fp);
    log_fp = NULL;
  }
  pthread_mutex_unlock(&log_lock);
}

static void format_time(char *buf, size_t max) {
  struct timespec ts;
  struct tm utc;
  clock_gettime(CLOCK_REALTIME, &ts);
  gmtime_r(&ts.tv_sec, &utc);
  size_t n = strftime(buf, max, LOG_TIME_FMT, &utc);
  snprintf(buf + n, max - n, ".%03ldZ", ts.tv_nsec / NS_PER_MS);
}

// sev < 0: data line without severity
static int vlog(int sev, log_src_t src, const char *fmt, va_list ap) {
  char stamp[LOG_TIME_MAX];
  char msg[CONF_LOG_LINE_MAX];
  char line[CONF_LOG_LINE_MAX];

  format_time(stamp, sizeof(stamp));
  vsnprintf(msg, sizeof(msg), fmt, ap);

  int n;
  if (sev < 0) {
    n = snprintf(line, sizeof(line), "%s %s %s", stamp, log_src_name(src), msg);
  } else {
    n = snprintf(line, sizeof(line), "%s %s %s %s", stamp, log_src_name(src),
                 log_sev_name((log_sev_t)sev), msg);
  }
  if (n < 0) {
    return -1;
  }
  // Longer lines are cut at CONF_LOG_LINE_MAX, on purpose

  pthread_mutex_lock(&log_lock);
  if (lora_queue.count >= CONF_LOG_LORA_QUEUE) {
    list_pop(&lora_queue);
    lora_dropped++;
  }
  if (!list_push(&lora_queue, line)) {
    lora_dropped++;
  }

  int rc;
  if (!log_fp) {
    rc = list_push(&pending, line) ? 0 : -1;
  } else {
    rc = write_line(line);
  }
  pthread_mutex_unlock(&log_lock);
  return rc;
}

#define LOG_DATA -1

int log_data(log_src_t src, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int rc = vlog(LOG_DATA, src, fmt, ap);
  va_end(ap);
  return rc;
}

int log_err(log_src_t src, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int rc = vlog(LOG_ERR, src, fmt, ap);
  va_end(ap);
  return rc;
}

int log_warn(log_src_t src, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int rc = vlog(LOG_WARN, src, fmt, ap);
  va_end(ap);
  return rc;
}

int log_info(log_src_t src, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int rc = vlog(LOG_INFO, src, fmt, ap);
  va_end(ap);
  return rc;
}

int log_lora_next(char *buf, size_t max) {
  pthread_mutex_lock(&log_lock);
  if (!lora_queue.head || max == 0) {
    pthread_mutex_unlock(&log_lock);
    return 0;
  }

  size_t n = strnlen(lora_queue.head->text, max - 1);
  memcpy(buf, lora_queue.head->text, n);
  buf[n] = '\0';
  list_pop(&lora_queue);
  pthread_mutex_unlock(&log_lock);
  return (int)n;
}
