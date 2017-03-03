/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "rulos.h"
#include "hardware.h"
#include "twi.h"

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>


#define NEED_BUS(twi) ((twi->out_pkt != NULL) || (twi->masterRecvSlot != NULL))

struct s_TwiState
{
	MediaStateIfc media;  // this must be first... :-(

	uint8_t initted;
	uint8_t have_bus;
	uint8_t dont_ack;

	// master-transmitter state
	const char *out_pkt;
	uint8_t out_len;
	uint8_t out_n;
	uint8_t out_destaddr;
	MediaSendDoneFunc sendDoneCB;
	void *sendDoneCBData;

	// slave-receiver state
	MediaRecvSlot *slaveRecvSlot;
	int8_t slaveRecvLen; // we use -1 as a sentinel so must be signed

	// master-receiver state
	MediaRecvSlot *masterRecvSlot;
	uint8_t masterRecvAddr;
	int8_t masterRecvLen; // we use -1 as a sentinel so must be signed
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


static void end_xmit(TwiState *twi)
{
	assert(twi->out_pkt != NULL);
	assert(twi->have_bus == TRUE);

	twi->out_pkt = NULL;

	if (twi->sendDoneCB != NULL) {
		// NB, the callback should run after sendDoneCB gets nulled out.
		// One of our sanity checks upon receiving a new packet to
		// transmit is that we don't have a pending callback.  However,
		// the callback is often where new packets get scheduled.
		schedule_now((ActivationFuncPtr) twi->sendDoneCB, twi->sendDoneCBData);
		twi->sendDoneCB = NULL;
		twi->sendDoneCBData = NULL;
	}
}


// A trampoline function for the receive upcall.  Scheduled by initiateRecvCallback, above.
static void doRecvCallback(MediaRecvSlot *mrs)
{
	assert(mrs != NULL);
	assert(mrs->func != NULL);
	assert(mrs->occupied_len > 0);

#ifdef DEBUG_STACK_WITH_UART
	hal_uart_sync_send("U", 1);
#endif
	mrs->func(mrs, mrs->occupied_len);
#ifdef DEBUG_STACK_WITH_UART
	hal_uart_sync_send("u", 1);
#endif
}


static void abortSlaveRecv(TwiState *twi)
{
	twi->slaveRecvLen = -1;
	twi->slaveRecvSlot->occupied_len = 0;
}


static void mr_maybe_dont_ack(TwiState *twi)
{
	if (twi->masterRecvSlot && twi->masterRecvLen == twi->masterRecvSlot->capacity-1)
		twi->dont_ack = TRUE;
}

static void end_mr(TwiState *twi)
{
	assert(twi->masterRecvSlot != NULL);
	twi->masterRecvSlot->occupied_len = twi->masterRecvLen;
	schedule_now((ActivationFuncPtr) doRecvCallback, twi->masterRecvSlot);
	twi->masterRecvSlot = NULL;
	twi->masterRecvLen = -1;
}

static void twi_set_control_register(TwiState *twi, uint8_t bits)
{
	bits |=
		_BV(TWEN) | // enable twi
		_BV(TWIE)   // enable twi interrupts;
		;

	if (!twi->dont_ack)
	{
		bits |=  _BV(TWEA); // ack incoming transmissions
	}
	twi->dont_ack = FALSE;

	// If we have a packet to master-transmit or master-receive, and
	// we're not in the start state, tell the CPU we want it to
	// acquire the bus.
	if (NEED_BUS(twi) && !twi->have_bus)
	{
		bits |= _BV(TWSTA);
	}

	// Conversely, if we're done with the bus and still have it, send
	// the stop state
	if (!NEED_BUS(twi) && twi->have_bus)
	{
		bits |= _BV(TWSTO);
		twi->have_bus = FALSE;
	}

	TWCR = bits;
}

static void twi_update(TwiState *twi, uint8_t status)
{
	assert(twi->initted == TWI_MAGIC);

	// Mask off the prescaler bits
	status &= 0b11111000;

#ifdef DEBUG_STACK_WITH_UART
	char c = twi->slaveRecvSlot->occupied_len;
	hal_uart_sync_send(&c, 1);
#endif

	switch (status) {

	case TW_REP_START:
	case TW_START:
		// if we just acquired the bus, decide if we're going into
		// master transmit or master receive mode.

		assert(twi->out_pkt != NULL || twi->masterRecvSlot != NULL);

		if (twi->out_pkt != NULL)
		{
			// master transmit
			TWDR = twi->out_destaddr << 1 | TW_WRITE;
			twi->out_n = 0;
			twi->have_bus = 1;
		}
		else if (twi->masterRecvSlot != NULL)
		{
			// master receive
			TWDR = twi->masterRecvAddr << 1 | TW_READ;
			twi->masterRecvLen = 0;
			twi->have_bus = 1;
		}
		else
		{
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
		assert(twi->out_pkt != NULL)

		if (twi->out_n >= twi->out_len)
		{
			end_xmit(twi);
		}
		else
		{
			TWDR = twi->out_pkt[twi->out_n++];
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
		if (twi->slaveRecvSlot != NULL && twi->slaveRecvSlot->occupied_len == 0 && twi->slaveRecvLen == -1)
		{
			twi->slaveRecvLen = 0;
		}
		else
		{
			LOGF((logfp, "dropping packet because receive buffer is busy"));
			twi->slaveRecvLen = -1;
			twi->dont_ack = TRUE;
		}
		break;


	case TW_SR_DATA_ACK:
		// one byte was received and acked.  If we're in "drop the
		// rest of this packet" mode, do nothing.  If we've overrun
		// the buffer, drop packet (including this byte).
		if (twi->slaveRecvLen == -1)
		{
			twi->dont_ack = TRUE;
			break;
		}

		// overflowed the receive buffer?  if so, drop entire packet.
		if (twi->slaveRecvSlot->capacity <= twi->slaveRecvLen)
		{
			LOGF((logfp, "dropping packet too long for local receive buffer"));
			abortSlaveRecv(twi);
			twi->dont_ack = TRUE;
			break;
		}

		// store next byte
		twi->slaveRecvSlot->data[twi->slaveRecvLen++] = TWDR;
		break;

	case TW_SR_DATA_NACK:
		// we didn't ack a byte for some reason.  if we're in
	  	// the middle of a packet, abandon it.
		if (twi->slaveRecvLen >= 0)
			abortSlaveRecv(twi);
		break;

	case TW_SR_STOP:
		// end of packet, yay!  upcall into network stack
		if (twi->slaveRecvLen > 0)
		{
			twi->slaveRecvSlot->occupied_len = twi->slaveRecvLen;
			schedule_now((ActivationFuncPtr) doRecvCallback, twi->slaveRecvSlot);
		}
		twi->slaveRecvLen = -1;
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
		if (twi->masterRecvSlot && twi->masterRecvLen < twi->masterRecvSlot->capacity)
		{
			twi->masterRecvSlot->data[twi->masterRecvLen++] = TWDR;

			// if this was the next-to-the-last byte, don't ack
			mr_maybe_dont_ack(twi);

			// if we've received the entire packet, send a NACK and
			// send the packet up.
			if (twi->masterRecvLen == twi->masterRecvSlot->capacity)
			{
				end_mr(twi);
			}
		}

		break;

	case TW_MR_SLA_NACK:
		// device didn't reply.  Just send up what we have.
		end_mr(twi);
		break;

	}
  
	// tell the TWI hardware we've processed the interrupt,
	// and acquire or release the bus as necessary.
	twi_set_control_register(twi, _BV(TWINT));
}

void _hal_twi_send(MediaStateIfc *media,
	Addr dest_addr, const char *data, uint8_t len,
	MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);

TwiState _twi_g;

ISR(TWI_vect)
{
	twi_update(&_twi_g, TW_STATUS);
}

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr, MediaRecvSlot *slaveRecvSlot)
{
	TwiState *twi = &_twi_g;

#ifdef DEBUG_STACK_WITH_UART
	hal_uart_init(NULL, 250000, 0);
#endif

	/* initialize our local state */
	memset(twi, 0, sizeof(TwiState));
	twi->media.send = &_hal_twi_send;
	twi->slaveRecvSlot = slaveRecvSlot;
	twi->slaveRecvLen = -1;

	twi->out_pkt = NULL;
	twi->out_len = 0;
	twi->sendDoneCB = NULL;

	twi->masterRecvSlot = NULL;
	twi->masterRecvLen = -1;

	twi->initted = TWI_MAGIC;

	/* convert khz to TWBR */
	TWBR = ((uint32_t) hardware_f_cpu - 16000 * speed_khz) / (2000 * speed_khz);

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
void _hal_twi_send(MediaStateIfc *media, Addr dest_addr, const char *data, uint8_t len, 
		   MediaSendDoneFunc sendDoneCB, void *sendDoneCBData)
{
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
void hal_twi_start_master_read(TwiState *twi, Addr addr, MediaRecvSlot *masterRecvSlot)
{
	assert(twi->initted == TWI_MAGIC);
	assert(masterRecvSlot != NULL);

	rulos_irq_state_t old_interrupts = hal_start_atomic();
	assert(twi->masterRecvSlot == NULL);

	twi->masterRecvSlot = masterRecvSlot;
	twi->masterRecvAddr = addr;
	twi_set_control_register(twi, 0);
	hal_end_atomic(old_interrupts);
}


