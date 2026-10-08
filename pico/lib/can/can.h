#pragma once

#include "can2040.h"
#include "config.h"
#include "types.h"
#include <stdbool.h>
#include <stdint.h>

#define CAN_STD_ID_MASK 0x7FFu
#define CAN_EXT_ID_MASK 0x1FFFFFFFu
#define CAN_IRQ_PRIORITY 1

typedef struct {
  struct can2040 cd;
  uint32_t pio, rx, tx, bitrate;
  can_frame_t rx_buf[CONF_CAN_RX_BUF];
  volatile uint32_t head, tail;
  uint32_t errors;
} can_t;

can_t *can_open(uint32_t pio, uint32_t rx, uint32_t tx, uint32_t bitrate);
void can_close(can_t *can);
int can_send(can_t *can, const can_frame_t *frame);
bool can_receive(can_t *can, can_frame_t *frame);
