#include "ubx.h"

#include <string.h>

enum {
  UBX_ST_SYNC1,
  UBX_ST_SYNC2,
  UBX_ST_CLASS,
  UBX_ST_ID,
  UBX_ST_LEN1,
  UBX_ST_LEN2,
  UBX_ST_PAYLOAD,
  UBX_ST_CK_A,
  UBX_ST_CK_B,
};

void ubx_open(ubx_t *ubx) { memset(ubx, 0, sizeof(*ubx)); }

void ubx_checksum(const uint8_t *data, size_t len, uint8_t *ck_a,
                  uint8_t *ck_b) {
  uint8_t a = 0, b = 0;
  for (size_t i = 0; i < len; i++) {
    a += data[i];
    b += a;
  }
  *ck_a = a;
  *ck_b = b;
}

size_t ubx_build(uint8_t *out, uint8_t cls, uint8_t id, const uint8_t *payload,
                 uint16_t len) {
  out[0] = UBX_SYNC1;
  out[1] = UBX_SYNC2;
  out[2] = cls;
  out[3] = id;
  out[4] = len & 0xFF;
  out[5] = len >> 8;
  if (len) {
    memcpy(&out[UBX_HEADER_LEN], payload, len);
  }
  // Checksum covers class, id, length and payload
  ubx_checksum(&out[2], len + 4, &out[UBX_HEADER_LEN + len],
               &out[UBX_HEADER_LEN + len + 1]);
  return UBX_FRAME_LEN(len);
}

static void ck_add(ubx_t *ubx, uint8_t byte) {
  ubx->ck_a += byte;
  ubx->ck_b += ubx->ck_a;
}

bool ubx_parse(ubx_t *ubx, uint8_t byte) {
  switch (ubx->state) {
  case UBX_ST_SYNC1:
    if (byte == UBX_SYNC1) {
      ubx->state = UBX_ST_SYNC2;
    }
    break;
  case UBX_ST_SYNC2:
    ubx->state = byte == UBX_SYNC2 ? UBX_ST_CLASS : UBX_ST_SYNC1;
    ubx->ck_a = ubx->ck_b = 0;
    break;
  case UBX_ST_CLASS:
    ubx->cls = byte;
    ck_add(ubx, byte);
    ubx->state = UBX_ST_ID;
    break;
  case UBX_ST_ID:
    ubx->id = byte;
    ck_add(ubx, byte);
    ubx->state = UBX_ST_LEN1;
    break;
  case UBX_ST_LEN1:
    ubx->len = byte;
    ck_add(ubx, byte);
    ubx->state = UBX_ST_LEN2;
    break;
  case UBX_ST_LEN2:
    ubx->len |= (uint16_t)byte << 8;
    ck_add(ubx, byte);
    ubx->pos = 0;
    if (ubx->len > UBX_MAX_PAYLOAD) {
      ubx->state = UBX_ST_SYNC1; // too long for us, drop
    } else {
      ubx->state = ubx->len ? UBX_ST_PAYLOAD : UBX_ST_CK_A;
    }
    break;
  case UBX_ST_PAYLOAD:
    ubx->payload[ubx->pos++] = byte;
    ck_add(ubx, byte);
    if (ubx->pos >= ubx->len) {
      ubx->state = UBX_ST_CK_A;
    }
    break;
  case UBX_ST_CK_A:
    ubx->state = byte == ubx->ck_a ? UBX_ST_CK_B : UBX_ST_SYNC1;
    break;
  case UBX_ST_CK_B:
    ubx->state = UBX_ST_SYNC1;
    return byte == ubx->ck_b;
  }
  return false;
}
