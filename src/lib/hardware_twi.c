/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#include "rocket.h"
#include "hardware.h"


struct twiState;

typedef struct {
	ActivationFunc func;
	struct twiState *twi;
} twiCallbackAct_t;

typedef struct twiState
{
	uint8_t initted;
	uint8_t have_bus;
	uint8_t need_bus;
	uint8_t done_with_bus;
	uint8_t dont_ack;

	// master-transmitter state
	char *out_pkt;
	uint8_t out_len;
	uint8_t out_n;
	uint8_t out_destaddr;
	TWISendDoneFunc sendDoneCB;
	void *sendDoneCBData;
	twiCallbackAct_t sendCallbackAct;

	// slave-receiver state
	TWIRecvSlot *trs;
	uint8_t in_n;
	uint8_t in_done;
	twiCallbackAct_t recvCallbackAct;

	// master-receiver state
	TWIRecvSlot *mr_recvSlot;
	uint8_t mr_addr;
	uint8_t mr_n;
	twiCallbackAct_t mrCallbackAct;
	
} twiState_t;

twiState_t twiState_g = {FALSE};

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


// This is a little trampoline function so that the send-done callback
// can be sent during schedule-time, rather than interrupt-time.
static void doSendCallback(twiCallbackAct_t *tca)
{
	// We store the callback and null it out before calling, because
	// one of our sanity checks upon receiving a new packet to
	// transmit is that we don't have a pending callback.  However,
	// the callback is often where new packets get scheduled.
	twiState_t *twi = tca->twi;
	TWISendDoneFunc cb = twi->sendDoneCB;
	twi->sendDoneCB = NULL;
	cb(twi->sendDoneCBData);
}

static void end_xmit(twiState_t *twi)
{
	assert(twi->out_pkt != NULL);

	twi->out_pkt = NULL;
	twi->done_with_bus = TRUE;
	schedule_now((Activation *) &twi->sendCallbackAct);
}

static void end_mr(twiState_t *twi)
{
	assert(twi->mr_recvSlot != NULL);

	twi->done_with_bus = TRUE;
	schedule_now((Activation *) &twi->mrCallbackAct);
}


// A trampoline function for the receive upcall.
static void doRecvCallback(twiCallbackAct_t *tca)
{
	twiState_t *twi = tca->twi;
	twi->trs->func(twi->trs, twi->in_n);
}

static void doMrCallback(twiCallbackAct_t *tca)
{
	twiState_t *twi = tca->twi;
	TWIRecvSlot *trs = twi->mr_recvSlot;
	uint8_t size = twi->mr_n;

	// free the stack to be able to receive the next packet before
	// calling the callback
	twi->mr_recvSlot = NULL;
	trs->func(trs, size);
}


static void abort_recv(twiState_t *twi)
{
	twi->in_n = -1;
	twi->in_done = FALSE;
	twi->trs->occupied = FALSE;
}



static void mr_maybe_dont_ack(twiState_t *twi)
{
	if (twi->mr_recvSlot && twi->mr_n == twi->mr_recvSlot->capacity-1)
		twi->dont_ack = TRUE;
}

static void twi_set_control_register(twiState_t *twi, uint8_t bits)
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
	if (twi->need_bus && !twi->have_bus)
	{
		bits |= _BV(TWSTA);
		twi->done_with_bus = FALSE;
	}

	// Conversely, if we're done with the bus and still have it, send
	// the stop state
	if (twi->have_bus && twi->done_with_bus)
	{
		bits |= _BV(TWSTO);
		twi->have_bus = twi->done_with_bus = FALSE;
	}

	TWCR = bits;
}


static void twi_update(twiState_t *twi, uint8_t status)
{
	assert(twi->initted == TWI_MAGIC);

	//print_status(status);

	switch (status) {

	case TW_REP_START:
	case TW_START:
		// if we just acquired the bus, decide if we're going into
		// master transmit or master receive mode.

		assert(twi->out_pkt != NULL || twi->mr_recvSlot != NULL);

		if (twi->out_pkt != NULL)
		{
			// master transmit
			TWDR = twi->out_destaddr << 1 | TW_WRITE;
			twi->out_n = 0;
			twi->have_bus = 1;
		}
		else if (twi->mr_recvSlot != NULL)
		{
			// master receive
			TWDR = twi->mr_addr << 1 | TW_READ;
			twi->mr_n = 0;
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
    // note -- master receiver arb lost does the same thing
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
		if (twi->trs && !twi->trs->occupied)
		{
			twi->trs->occupied = TRUE;
			twi->in_done = FALSE;
			twi->in_n = 0;
		}
		else
		{
			LOGF((logfp, "dropping packet because receive buffer is busy"));
			// don't set in_n to -1!  there might be a valid packet
			// still on its way up the stack.
		}
		break;


	case TW_SR_DATA_ACK:
		// one byte was received and acked.  If we're in "drop the
		// rest of this packet" mode, do nothing.  If we've overrun
		// the buffer, drop packet (including this byte).
		if (twi->in_n == -1)
		{
			break;
		}

		// overflowed the receive buffer?  if so, drop entire packet.
		if (twi->trs->capacity < twi->in_n)
		{
			LOGF((logfp, "dropping packet too long for local receive buffer"));
			abort_recv(twi);
			break;
		}

		// store next byte
		twi->trs->data[twi->in_n++] = TWDR;
		break;

	case TW_SR_DATA_NACK:
		// we didn't ack a byte for some reason; abandon the packet
		abort_recv(twi);
		break;

	case TW_SR_STOP:
		// end of packet, yay!  upcall into network stack
		if (!twi->in_done && twi->in_n > 0)
		{
			twi->in_done = TRUE;
			schedule_now((Activation *) &twi->recvCallbackAct);
		}
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
		// received a byte!
		if (twi->mr_recvSlot && twi->mr_n < twi->mr_recvSlot->capacity)
		{
			twi->mr_recvSlot->data[twi->mr_n++] = TWDR;

			// if this was the next-to-the-last byte, don't ack
			mr_maybe_dont_ack(twi);

			// if we've received the entire packet, send a NACK and
			// send the packet up.
			if (twi->mr_n == twi->mr_recvSlot->capacity)
			{
				end_mr(twi);
			}
		}

		break;

	case TW_MR_SLA_NACK:
	case TW_MR_DATA_NACK:
		// device didn't reply, or stopped giving us data halfway
		// through.  Just send up what we have.
		end_mr(twi);
		break;

	}
  
	// tell the TWI hardware we've processed the interrupt,
	// and acquire or release the bus as necessary.
	twi_set_control_register(twi, _BV(TWINT));
}


ISR(TWI_vect)
{
	twi_update(&twiState_g, TW_STATUS);
}

void hal_twi_init(Addr local_addr, TWIRecvSlot *trs)
{
	/* initialize our local state */
	twiState_g.trs = trs;
	twiState_g.have_bus = FALSE;
	twiState_g.out_pkt = NULL;
	twiState_g.out_len = 0;
	twiState_g.sendDoneCB = NULL;
	twiState_g.sendCallbackAct.func = (ActivationFunc) doSendCallback; 
	twiState_g.sendCallbackAct.twi = &twiState_g;

	twiState_g.in_n = -1;
	twiState_g.recvCallbackAct.func = (ActivationFunc) doRecvCallback;
	twiState_g.recvCallbackAct.twi = &twiState_g;

	twiState_g.mr_recvSlot = NULL;
	twiState_g.mr_n = -1;
	twiState_g.mrCallbackAct.func = (ActivationFunc) doMrCallback;
	twiState_g.mrCallbackAct.twi = &twiState_g;

	twiState_g.initted = TWI_MAGIC;

	/* set 100khz (assuming 8mhz local clock!  fix me...) */
	TWBR = 32;

	/* configure the local address */
	TWAR = local_addr << 1;

	/* set the mask to be all 0, meaning all bits of the address are
	 * significant */
#if defined(MCUatmega328p)
	TWAMR = 0;
#elif defined(MCUatmega8)

#endif

	/* enable interrupts */
	sei();

	/* enable twi */
	twi_set_control_register(&twiState_g, 0);
}

// Send a packet in master-transmitter mode.
void hal_twi_send(Addr dest_addr, char *data, uint8_t len, 
				  TWISendDoneFunc sendDoneCB, void *sendDoneCBData)
{
	uint8_t old_interrupts = hal_start_atomic();
	assert(data != NULL);
	assert(len > 0);
	assert(twiState_g.initted == TWI_MAGIC);
	assert(twiState_g.out_pkt == NULL);
	assert(twiState_g.sendDoneCB == NULL);

	twiState_g.out_pkt = data;
	twiState_g.out_destaddr = dest_addr;
	twiState_g.out_len = len;
	twiState_g.sendDoneCB = sendDoneCB;
	twiState_g.sendDoneCBData = sendDoneCBData;

	twiState_g.need_bus = TRUE;
	twi_set_control_register(&twiState_g, 0);
	hal_end_atomic(old_interrupts);
}

// Read a packet in master-receiver mode.
void hal_twi_read(Addr addr, TWIRecvSlot *trs)
{
	uint8_t old_interrupts = hal_start_atomic();
	assert(trs != NULL);
	twiState_g.mr_recvSlot = trs;
	twiState_g.mr_addr = addr;

	twiState_g.need_bus = TRUE;
	twi_set_control_register(&twiState_g, 0);
	hal_end_atomic(old_interrupts);
}

