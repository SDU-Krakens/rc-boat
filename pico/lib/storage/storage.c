#include "storage.h"
#include "config.h"
#include "log.h"

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Region layout:
//   storage_header_t, then entries back to back:
//   storage_entry_t, data[len], padding to STORAGE_ALIGN
// Unused space is 0xFF (erased flash).
#define STORAGE_MAGIC 0x4B524B53u // "SKRK"
#define STORAGE_VERSION 1
#define STORAGE_ALIGN 4
#define STORAGE_FLASH_TIMEOUT_MS 1000
#define STORAGE_ERASED_TAG 0xFFFF

#define CRC32_INIT 0xFFFFFFFFu
#define CRC32_POLY 0xEDB88320u

_Static_assert(CONF_STORAGE_SIZE % FLASH_SECTOR_SIZE == 0,
               "CONF_STORAGE_SIZE must be a multiple of the flash sector size");
_Static_assert(CONF_STORAGE_SIZE < PICO_FLASH_SIZE_BYTES,
               "CONF_STORAGE_SIZE must be smaller than the flash");

typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
} storage_header_t;

typedef struct {
  uint16_t tag;
  uint16_t len;
  uint32_t crc;
} storage_entry_t;

typedef struct {
  uint32_t offset;
  const uint8_t *data;
} storage_write_t;

static uint32_t crc32(const uint8_t *data, size_t len) {
  uint32_t crc = CRC32_INIT;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 1) ? (crc >> 1) ^ CRC32_POLY : crc >> 1;
    }
  }
  return ~crc;
}

static size_t align_up(size_t n) {
  return (n + STORAGE_ALIGN - 1) & ~(size_t)(STORAGE_ALIGN - 1);
}

static const uint8_t *region(storage_t *st) {
  return (const uint8_t *)(XIP_BASE + st->offset);
}

static bool header_valid(const uint8_t *base) {
  const storage_header_t *hdr = (const storage_header_t *)base;
  return hdr->magic == STORAGE_MAGIC && hdr->version == STORAGE_VERSION;
}

// Find the entry with 'tag' in a region image, returns its byte offset or -1.
// '*end' receives the offset just after the last entry.
static long find_entry(const uint8_t *base, uint16_t tag, size_t *end) {
  size_t pos = sizeof(storage_header_t);
  long found = -1;

  while (pos + sizeof(storage_entry_t) <= CONF_STORAGE_SIZE) {
    const storage_entry_t *e = (const storage_entry_t *)(base + pos);
    if (e->tag == STORAGE_ERASED_TAG) {
      break;
    }
    size_t next = pos + align_up(sizeof(storage_entry_t) + e->len);
    if (next > CONF_STORAGE_SIZE) {
      break;
    }
    if (e->tag == tag) {
      found = (long)pos;
    }
    pos = next;
  }

  if (end) {
    *end = pos;
  }
  return found;
}

static void flash_write_cb(void *param) {
  storage_write_t *w = param;
  flash_range_erase(w->offset, CONF_STORAGE_SIZE);
  flash_range_program(w->offset, w->data, CONF_STORAGE_SIZE);
}

storage_t *storage_open(void) {
  storage_t *st = malloc(sizeof(storage_t));
  if (!st) {
    return NULL;
  }
  st->offset = PICO_FLASH_SIZE_BYTES - CONF_STORAGE_SIZE;

  log_info(LOG_SRC_STORAGE, "open, offset %08lx size %u valid %d",
           (unsigned long)st->offset, (unsigned)CONF_STORAGE_SIZE,
           header_valid(region(st)));

  return st;
}

void storage_close(storage_t *st) { free(st); }

int storage_save(storage_t *st, uint16_t tag, const void *data, size_t len) {
  if (tag == STORAGE_ERASED_TAG || len > UINT16_MAX) {
    return -1;
  }

  uint8_t *img = malloc(CONF_STORAGE_SIZE);
  if (!img) {
    return -1;
  }
  memset(img, 0xFF, CONF_STORAGE_SIZE);

  storage_header_t hdr = {STORAGE_MAGIC, STORAGE_VERSION, 0};
  memcpy(img, &hdr, sizeof(hdr));
  size_t pos = sizeof(hdr);

  // Copy every other existing entry into the new image
  const uint8_t *base = region(st);
  if (header_valid(base)) {
    size_t end;
    find_entry(base, STORAGE_ERASED_TAG, &end);
    size_t src = sizeof(storage_header_t);
    while (src < end) {
      const storage_entry_t *e = (const storage_entry_t *)(base + src);
      size_t size = align_up(sizeof(storage_entry_t) + e->len);
      if (e->tag != tag) {
        memcpy(img + pos, base + src, size);
        pos += size;
      }
      src += size;
    }
  }

  size_t size = align_up(sizeof(storage_entry_t) + len);
  if (pos + size > CONF_STORAGE_SIZE) {
    free(img);
    return -1;
  }

  storage_entry_t e = {tag, (uint16_t)len, crc32(data, len)};
  memcpy(img + pos, &e, sizeof(e));
  memcpy(img + pos + sizeof(e), data, len);

  storage_write_t w = {st->offset, img};
  int rc = flash_safe_execute(flash_write_cb, &w, STORAGE_FLASH_TIMEOUT_MS);
  free(img);

  if (rc != PICO_OK) {
    log_err(LOG_SRC_STORAGE, "save tag %u len %u failed (%d)", tag,
            (unsigned)len, rc);
    return -1;
  }
  log_info(LOG_SRC_STORAGE, "saved tag %u len %u", tag, (unsigned)len);
  return 0;
}

int storage_load(storage_t *st, uint16_t tag, void *data, size_t len) {
  const uint8_t *base = region(st);
  if (!header_valid(base)) {
    return -1;
  }

  long pos = find_entry(base, tag, NULL);
  if (pos < 0) {
    return -1;
  }

  const storage_entry_t *e = (const storage_entry_t *)(base + pos);
  const uint8_t *payload = base + pos + sizeof(storage_entry_t);
  if (e->len != len || crc32(payload, e->len) != e->crc) {
    log_warn(LOG_SRC_STORAGE, "load tag %u invalid", tag);
    return -1;
  }

  memcpy(data, payload, len);
  return 0;
}
