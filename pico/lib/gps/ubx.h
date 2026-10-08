#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UBX_SYNC1 0xB5
#define UBX_SYNC2 0x62
#define UBX_HEADER_LEN 6 // sync1, sync2, class, id, len (2)
#define UBX_CHECKSUM_LEN 2
#define UBX_FRAME_LEN(payload) (UBX_HEADER_LEN + (payload) + UBX_CHECKSUM_LEN)
#define UBX_MAX_PAYLOAD 100

// Classes and ids
#define UBX_CLASS_NAV 0x01
#define UBX_CLASS_ACK 0x05
#define UBX_CLASS_CFG 0x06
#define UBX_NAV_PVT 0x07
#define UBX_ACK_NAK 0x00
#define UBX_ACK_ACK 0x01
#define UBX_CFG_PRT 0x00
#define UBX_CFG_MSG 0x01
#define UBX_CFG_RATE 0x08

typedef struct {
  uint8_t state;
  uint8_t cls, id;
  uint16_t len, pos;
  uint8_t ck_a, ck_b;
  uint8_t payload[UBX_MAX_PAYLOAD];
} ubx_t;

void ubx_open(ubx_t *ubx);
size_t ubx_build(uint8_t *out, uint8_t cls, uint8_t id, const uint8_t *payload,
                 uint16_t len);
void ubx_checksum(const uint8_t *data, size_t len, uint8_t *ck_a,
                  uint8_t *ck_b);
bool ubx_parse(ubx_t *ubx, uint8_t byte);
