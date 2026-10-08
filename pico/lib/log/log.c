#include "log.h"
#include "config.h"

#include "pico/time.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define US_PER_MS 1000

// Line waiting to be confirmed by the Zero
typedef struct log_entry {
  struct log_entry *next;
  log_sev_t sev;
  log_src_t src;
  char text[];
} log_entry_t;

static comm_t *log_comm;
static log_entry_t *head, *tail; // oldest first, head is the one in flight
static size_t queued_bytes;
static bool in_flight;
static uint8_t in_flight_counter;
static uint64_t sent_us;

static void pop(void) {
  log_entry_t *e = head;
  head = e->next;
  if (!head) {
    tail = NULL;
  }
  queued_bytes -= strlen(e->text);
  free(e);
}

static void send_head(void) {
  int counter = comm_send_text(log_comm, head->sev, head->src, "%s", head->text);
  if (counter < 0) {
    return; // retried on the next log_poll
  }
  in_flight = true;
  in_flight_counter = (uint8_t)counter;
  sent_us = time_us_64();
}

static void on_ack(void *ctx, comm_type_t type, const void *msg) {
  (void)ctx;
  (void)type;
  uint8_t counter = *(const uint8_t *)msg;
  if (!head || !in_flight || counter != in_flight_counter) {
    return;
  }
  pop();
  in_flight = false;
  if (head) {
    send_head();
  }
}

int log_open(comm_t *comm) {
  log_comm = comm;
  comm_on(comm, COMM_TEXT_ACK, on_ack, NULL);
  return 0;
}

void log_close(void) {
  if (log_comm) {
    comm_on(log_comm, COMM_TEXT_ACK, NULL, NULL);
  }
  while (head) {
    pop();
  }
  in_flight = false;
  log_comm = NULL;
}

void log_poll(void) {
  if (!log_comm || !head) {
    return;
  }
  if (!in_flight) {
    send_head();
    return;
  }
  if (time_us_64() - sent_us >= (uint64_t)CONF_COMM_RESEND_MS * US_PER_MS) {
    comm_resend_text(log_comm, in_flight_counter, head->sev, head->src, "%s",
                     head->text);
    sent_us = time_us_64();
  }
}

bool log_full(void) { return queued_bytes >= CONF_PICO_LOG_QUEUE; }

// Always queues, even past CONF_PICO_LOG_QUEUE; log_full tells the main to pause
static int vlog(log_sev_t sev, log_src_t src, const char *fmt, va_list ap) {
  va_list copy;
  va_copy(copy, ap);
  int n = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);
  if (n < 0) {
    return -1;
  }

  log_entry_t *e = malloc(sizeof(log_entry_t) + n + 1);
  if (!e) {
    return -1;
  }
  e->next = NULL;
  e->sev = sev;
  e->src = src;
  vsnprintf(e->text, n + 1, fmt, ap);

  if (tail) {
    tail->next = e;
  } else {
    head = e;
  }
  tail = e;
  queued_bytes += n;

  log_poll();
  return 0;
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
