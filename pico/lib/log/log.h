#pragma once

#include "comm.h"
#include "log_src.h"

#include <stdbool.h>

// One log for the whole program; lines go to the Zero as text messages
int log_open(comm_t *comm);
void log_close(void);
int log_err(log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int log_warn(log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int log_info(log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void log_poll(void);
bool log_full(void);
