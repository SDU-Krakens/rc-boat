#pragma once

#include "config.h"
#include "log_src.h"

#include <stddef.h>

#define LOG_TIME_MAX 32 // "2026-09-24T15:04:05.123Z"

// One log for the whole program
int log_open(const char *dir);
void log_close(void);
int log_err(log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int log_warn(log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int log_info(log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
// Data line: time source text, no severity
int log_data(log_src_t src, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
int log_lora_next(char *buf, size_t max);
