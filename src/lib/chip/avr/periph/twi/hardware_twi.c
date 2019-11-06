/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/twi.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "core/twi.h"

#define NEED_BUS(twi) ((twi->out_pkt != NULL) || (twi->masterRecvSlot != NULL))

struct s_TwiState {
  MediaStateIfc media;  // this must be first... :-(

  uint8_t initted;
  uint8_t have_bus;
  uint8_t dont_ack;

  // master-transmitter state
  const void *out_pkt;
  uint8_t out_len;
  uint8_t out_n;
  uint8_t out_destaddr;
  MediaSendDoneFunc sendDoneCB;
  void *sendDoneCBData;

  // slave-receiver state
  MediaRecvSlot *slaveRecvSlot;

  // master-receiver state
  MediaRecvSlot *masterRecvSlot;
  uint8_t masterRecvAddr;
  int8_t masterRecvLen;  // we use -1 as a sentinel so must be signed
};

#define TWI_MAGIC 0x82

#if 0
static void print_status(uint16_t status)
{
	static uint16_t last_status = 999;
	SSBitmap bm[8];
	char buf[8];

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "St%3d%3d", last_status, status);
	last_status = status;
	ascii_to_bitmap_str(bm, 8, buf);
	program_board(2, bm);
}
#endif

static void end_xmit(TwiState *const twi) {
  assert(twi->out_pkt != NULL);
  assert(twi->have_bus == TRUE);

  twi->out_pkt = NULL;

  if (twi->sendDoneCB != NULL) {
    // NB, the callback should run after sendDoneCB gets nulled out.
    // One of our sanity checks upon receiving a new packet to
    // transmit is that we don't have a pending callback.  However,
    // the callback is often where new packets get scheduled.
    schedule_now((ActivationFuncPtr)twi->sendDoneCB, twi->sendDoneCBData);
    twi->sendDoneCB = NULL;
    twi->sendDoneCBData = NULL;
  }
}

static inline bool sr_is_valid_packet(MediaRecvSlot *slaveRecvSlot) {
  return slaveRecvSlot->packet_len > 0 &&
         slaveRecvSlot->packet_len <= slaveRecvSlot->capacity;
}

static inline void sr_deliver(MediaRecvSlot *slaveRecvSlot) {
  if (sr_is_valid_packet(slaveRecvSlot)) {
    slaveRecvSlot->func(slaveRecvSlot);
  }
  slaveRecvSlot->packet_len = 0;
}

static inline void mark_packet_invalid(MediaRecvSlot *recvSlot) {
  recvSlot->packet_len = recvSlot->capacity + 1;
}

static void mr_maybe_dont_ack(TwiState *const twi) {
  if (twi->masterRecvSlot &&
      twi->masterRecvLen == twi->masterRecvSlot->capacity - 1)
    twi->dont_ack = TRUE;
}

static void mr_deliver(TwiState *const twi) {
  MediaRecvSlot *const masterRecvSlot = twi->masterRecvSlot;
  assert(masterRecvSlot != NULL);

  // Let the generic socket layer dispatch the new data at interrupt time.
  masterRecvSlot->func(masterRecvSlot);

  // Caller is responsible to hal_twi_start_master_read for the next receive
  // request.
  twi->masterRecvSlot = NULL;
}

static void twi_set_control_register(TwiState *const twi, uint8_t bits) {
  bits |= _BV(TWEN) |  // enable twi
          _BV(TWIE)    // enable twi interrupts;
      ;

  if (!twi->dont_ack) {
    bits |= _BV(TWEA);  // ack incoming transmissions
  }
  twi->dont_ack = FALSE;

  // If we have a packet to master-transmit or master-receive, and
  // we're not in the start state, tell the CPU we want it to
  // acquire the bus.
  if (NEED_BUS(twi) && !twi->have_bus) {
    bits |= _BV(TWSTA);
  }

  // Conversely, if we're done with the bus and still have it, send
  // the stop state
  if (!NEED_BUS(twi) && twi->have_bus) {
    bits |= _BV(TWSTO);
    twi->have_bus = FALSE;
  }

  TWCR = bits;
}

static void twi_update(TwiState *const twi, uint8_t status) {
  assert(twi->initted == TWI_MAGIC);

  // Mask off the prescaler bits
  status &= 0b11111000;

  MediaRecvSlot *const slaveRecvSlot = twi->slaveRecvSlot;
  MediaRecvSlot *const masterRecvSlot = twi->masterRecvSlot;

  switch (status) {
    case TW_REP_START:
    case TW_START:
      // if we just acquired the bus, decide if we're going into
      // master transmit or master receive mode.

      if (twi->out_pkt != NULL) {
        // master transmit
        TWDR = twi->out_destaddr << 1 | TW_WRITE;
        twi->out_n = 0;
        twi->have_bus = 1;
      } else if (masterRecvSlot != NULL) {
        // master receive
        TWDR = twi->masterRecvAddr << 1 | TW_READ;
        masterRecvSlot->packet_len = 0;
        twi->have_bus = 1;
      } else {
        assert(FALSE);
      }

      break;

    //
    // Master Transmitter
    //
    case TW_MT_SLA_NACK:
    case TW_MT_DATA_NACK:
      // the guy on the other end apparently isn't listening.
      // Abandon the packet.
      end_xmit(twi);
      break;

    case TW_MT_ARB_LOST:
      // note -- master receiver arb lost does the same thing.
      // we were in the middle of a transmission and something bad
      // happened.  Re-acquire the bus.
      twi->have_bus = 0;
      break;

    case TW_MT_SLA_ACK:
    case TW_MT_DATA_ACK:
      // either the address byte, or a byte of the packet, was
      // acked.  Either way, xmit the next byte, if any.  Otherwise
      // we're done.
      assert(twi->out_pkt != NULL);

      if (twi->out_n >= twi->out_len) {
        end_xmit(twi);
      } else {
        TWDR = ((unsigned char *) twi->out_pkt)[twi->out_n++];
      }
      break;

    //
    // Slave Receiver
    //
    case TW_SR_ARB_LOST_SLA_ACK:
      // we were trying to get the bus, and instead another master
      // started sending a packet to us
      twi->have_bus = 0;
      // fall through to the case of SLA_ACK even in the case of no
      // arb loss

    case TW_SR_SLA_ACK:
      // new packet arriving addressed to us.  If the receive buffer
      // is available, start receiving.
      if (twi->slaveRecvSlot == NULL || twi->slaveRecvSlot->packet_len != 0) {
        LOG("dropping packet because receive buffer is busy");
        twi->dont_ack = TRUE;
      }
      // Ready to receive: packet_len accumulator is zero.
      break;

    case TW_SR_DATA_ACK:
      // overflowed the receive buffer?  if so, drop entire packet.
      if (slaveRecvSlot->packet_len >= slaveRecvSlot->capacity) {
        LOG("dropping packet too long for local receive buffer");
        twi->dont_ack = TRUE;
        mark_packet_invalid(slaveRecvSlot);
      } else {
        // store next byte
        slaveRecvSlot->data[slaveRecvSlot->packet_len++] = TWDR;
      }
      break;

    case TW_SR_DATA_NACK:
      // we didn't ack a byte for some reason.  if we're in
      // the middle of a packet, abandon it.
      mark_packet_invalid(slaveRecvSlot);
      break;

    case TW_SR_STOP:
      // end of packet, yay!  upcall into network stack
      sr_deliver(slaveRecvSlot);
      break;

    case TW_SR_ARB_LOST_GCALL_ACK:
      // we were trying to get the bus, and instead another master
      // started sending a packet to the general call address.  Drop
      // the incoming packet; we don't receive broadcasts (yet).
      twi->have_bus = 0;
      break;

    case TW_SR_GCALL_ACK:
    case TW_SR_GCALL_DATA_ACK:
    case TW_SR_GCALL_DATA_NACK:
      // Broadcast packet has started; ignore it (we don't support
      // broadcast)
      break;

    //
    // Master receiver
    //
    case TW_MR_SLA_ACK:
      // Packet is arriving in master receiver mode.  Either set or
      // clear the ack bit depending on how long the packet is.
      mr_maybe_dont_ack(twi);
      break;

    case TW_MR_DATA_ACK:
    case TW_MR_DATA_NACK:
      // received a byte -- that we might have acked, or not acked
      if (masterRecvSlot == NULL ||
          masterRecvSlot->packet_len >= twi->masterRecvSlot->capacity) {
        mark_packet_invalid(masterRecvSlot);
      } else {
        // store next byte
        slaveRecvSlot->data[slaveRecvSlot->packet_len++] = TWDR;

        // if this was the next-to-the-last byte, don't ack
        mr_maybe_dont_ack(twi);

        // if we've received the entire packet, send a NACK and
        // send the packet up.
        if (masterRecvSlot->packet_len == masterRecvSlot->capacity) {
          mr_deliver(twi);
        }
      }

      break;

    case TW_MR_SLA_NACK:
      // device didn't reply.  Just send up what we have.
      mr_deliver(twi);
      break;
  }

  // tell the TWI hardware we've processed the interrupt,
  // and acquire or release the bus as necessary.
  twi_set_control_register(twi, _BV(TWINT));
}

static void hal_twi_send(MediaStateIfc *media, Addr dest_addr,
                         const void *data, uint8_t len,
                         MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);

static TwiState g_twi;

ISR(TWI_vect) { twi_update(&g_twi, TW_STATUS); }

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr,
                            MediaRecvSlot *const slaveRecvSlot) {
  TwiState *const twi = &g_twi;

  /* initialize our local state */
  memset(twi, 0, sizeof(TwiState));
  twi->media.send = &hal_twi_send;
  twi->slaveRecvSlot = slaveRecvSlot;
  if (twi->slaveRecvSlot != NULL) {
    // We use packet_len>capacity to signal a receive overrun.
    assert(twi->slaveRecvSlot->capacity < 255);
    twi->slaveRecvSlot->packet_len = 0;
  }

  twi->out_pkt = NULL;
  twi->out_len = 0;
  twi->sendDoneCB = NULL;

  twi->masterRecvSlot = NULL;
  twi->masterRecvLen = -1;

  twi->initted = TWI_MAGIC;

  /* convert khz to TWBR */
  TWBR = ((uint32_t)hardware_f_cpu - 16000 * speed_khz) / (2000 * speed_khz);

  /* configure the local address */
  TWAR = local_addr << 1;

  /* set the mask to be all 0, meaning all bits of the address are
   * significant.  not all platforms support this. */
#if defined(TWAMR)
  TWAMR = 0;
#endif

  /* enable interrupts */
  sei();

  /* enable twi */
  twi_set_control_register(twi, 0);
  return &twi->media;
}

// Send a packet in master-transmitter mode.
static void hal_twi_send(MediaStateIfc *media, Addr dest_addr,
                         const void *data, uint8_t len,
                         MediaSendDoneFunc sendDoneCB, void *sendDoneCBData) {
  TwiState *twi = (TwiState *)media;
  assert(twi->initted == TWI_MAGIC);

  rulos_irq_state_t old_interrupts = hal_start_atomic();
  assert(data != NULL);
  assert(len > 0);
  assert(twi->out_pkt == NULL);
  assert(twi->sendDoneCB == NULL);
  assert(twi->have_bus == FALSE);

  twi->out_pkt = data;
  twi->out_destaddr = dest_addr;
  twi->out_len = len;
  twi->sendDoneCB = sendDoneCB;
  twi->sendDoneCBData = sendDoneCBData;

  twi_set_control_register(twi, 0);
  hal_end_atomic(old_interrupts);
}

// Read a packet in master-receiver mode.
void hal_twi_start_master_read(TwiState *twi, Addr addr,
                               MediaRecvSlot *masterRecvSlot) {
  assert(twi->initted == TWI_MAGIC);
  assert(masterRecvSlot != NULL);

  rulos_irq_state_t old_interrupts = hal_start_atomic();
  assert(twi->masterRecvSlot == NULL);

  twi->masterRecvSlot = masterRecvSlot;
  twi->masterRecvAddr = addr;
  twi_set_control_register(twi, 0);
  hal_end_atomic(old_interrupts);
}
