#include "can.h"
#include "log.h"

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include <stdlib.h>
#include <string.h>

// can2040 drives the PIO irq directly, so only one instance can exist
static can_t *can_instance;

static uint32_t pio_irq(uint32_t pio) { return pio ? PIO1_IRQ_0 : PIO0_IRQ_0; }

static void can_irq_handler(void) {
  can2040_pio_irq_handler(&can_instance->cd);
}

static void can_rx_cb(struct can2040 *cd, uint32_t notify,
                      struct can2040_msg *msg) {
  can_t *can = can_instance;
  (void)cd;

  if (notify & CAN2040_NOTIFY_ERROR) {
    can->errors++;
    return;
  }
  if (!(notify & CAN2040_NOTIFY_RX)) {
    return;
  }

  // Buffer full: drop the oldest frame
  if (can->head - can->tail >= CONF_CAN_RX_BUF) {
    can->tail++;
    can->errors++;
  }

  can_frame_t *f = &can->rx_buf[can->head % CONF_CAN_RX_BUF];
  f->ext = msg->id & CAN2040_ID_EFF;
  f->rtr = msg->id & CAN2040_ID_RTR;
  f->id = msg->id & (f->ext ? CAN_EXT_ID_MASK : CAN_STD_ID_MASK);
  f->dlc = msg->dlc > CAN_MAX_DLC ? CAN_MAX_DLC : msg->dlc;
  memcpy(f->data, msg->data, CAN_MAX_DLC);
  can->head++;
}

can_t *can_open(uint32_t pio, uint32_t rx, uint32_t tx, uint32_t bitrate) {
  if (can_instance) {
    return NULL;
  }

  can_t *can = calloc(1, sizeof(can_t));
  if (!can) {
    return NULL;
  }
  can->pio = pio;
  can->rx = rx;
  can->tx = tx;
  can->bitrate = bitrate;
  can_instance = can;

  can2040_setup(&can->cd, pio);
  can2040_callback_config(&can->cd, can_rx_cb);

  uint32_t irq = pio_irq(pio);
  irq_set_exclusive_handler(irq, can_irq_handler);
  irq_set_priority(irq, CAN_IRQ_PRIORITY);
  irq_set_enabled(irq, true);

  can2040_start(&can->cd, clock_get_hz(clk_sys), bitrate, rx, tx);

  log_info(LOG_SRC_CAN, "open, pio %lu rx %lu tx %lu bitrate %lu",
           (unsigned long)pio, (unsigned long)rx, (unsigned long)tx,
           (unsigned long)bitrate);
  return can;
}

void can_close(can_t *can) {
  if (!can) {
    return;
  }
  uint32_t irq = pio_irq(can->pio);
  can2040_stop(&can->cd);
  irq_set_enabled(irq, false);
  irq_remove_handler(irq, can_irq_handler);
  can_instance = NULL;
  free(can);
}

int can_send(can_t *can, const can_frame_t *frame) {
  if (frame->dlc > CAN_MAX_DLC) {
    return -1;
  }

  struct can2040_msg msg = {0};
  msg.id = frame->id & (frame->ext ? CAN_EXT_ID_MASK : CAN_STD_ID_MASK);
  if (frame->ext) {
    msg.id |= CAN2040_ID_EFF;
  }
  if (frame->rtr) {
    msg.id |= CAN2040_ID_RTR;
  }
  msg.dlc = frame->dlc;
  memcpy(msg.data, frame->data, frame->dlc);

  return can2040_transmit(&can->cd, &msg) == 0 ? 0 : -1;
}

bool can_receive(can_t *can, can_frame_t *frame) {
  bool got = false;

  // The rx callback may move tail when it drops the oldest frame
  uint32_t irq_state = save_and_disable_interrupts();
  if (can->tail != can->head) {
    *frame = can->rx_buf[can->tail % CONF_CAN_RX_BUF];
    can->tail++;
    got = true;
  }
  restore_interrupts(irq_state);

  return got;
}
