#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t offset; // flash offset of the reserved region
} storage_t;

storage_t *storage_open(void);
void storage_close(storage_t *st);
int storage_save(storage_t *st, uint16_t tag, const void *data, size_t len);
int storage_load(storage_t *st, uint16_t tag, void *data, size_t len);
