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

	// sending state
	char *out_pkt;
	uint8_t out_len;
	uint8_t out_n;
	uint8_t out_destaddr;
	TWISendDoneFunc sendDoneCB;
	void *sendDoneCBData;
	twiCallbackAct_t sendCallbackAct;

	// receiving state
	TWIRecvSlot *trs;
	uint8_t in_n;
	twiCallbackAct_t recvCallbackAct;
} twiState_t;

twiState_t twiState_g = {FALSE};


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
	twiState_t *twi = tca->twi;
	twi->sendDoneCB(twi->sendDoneCBData);
}

static void end_xmit(twiState_t *twi)
{
	twi->out_pkt = NULL;
	schedule_now((Activation *) &twi->sendCallbackAct);
}

// This is a similar trampoline function for the receive upcall.
static void doRecvCallback(twiCallbackAct_t *tca)
{
	twiState_t *twi = tca->twi;
	twi->trs->func(twi->trs, twi->in_n);
}


static void abort_recv(twiState_t *twi)
{
	twi->in_n = -1;
	twi->trs->occupied = FALSE;
}



static void twi_acquire_or_release_bus(twiState_t *twi)
{
	// If we have a packet to transmit, and we're not in the start
	// state, tell the CPU we want it to acquire the bus.
	if (twi->out_pkt != NULL && !twi->have_bus)
	{
		reg_set(&TWCR, TWSTA);
	}

	// Conversely, if we're done sending the packet and are still in
	// the start state, send the stop state
	if (twi->out_pkt == NULL && twi->have_bus) {
		reg_set(&TWCR, TWSTO);
		twi->have_bus = 0;
	}
}


static void twi_update(twiState_t *twi, uint8_t status)
{
	if (!twi->initted)
		return;

	//  print_status(status);

	switch (status) {

	//
	// Master Transmitter
	//
	case TW_REP_START:
	case TW_START:
		// if we just acquired the bus, send the dest. address
		TWDR = twi->out_destaddr << 1 | TW_WRITE;
		twi->have_bus = 1;
		break;

	case TW_MT_SLA_NACK:
	case TW_MT_DATA_NACK:
		// the guy on the other end apparently isn't listening.
		// Abandon the packet.
		end_xmit(twi);
		break;

	case TW_MT_ARB_LOST:
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
		if (!twi->trs->occupied)
		{
			twi->trs->occupied = TRUE;
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
		if (twi->in_n > 0)
		{
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
	}
  
	// tell the TWI hardware we've processed the interrupt
	reg_set(&TWCR, TWINT);

	// acquire or release the bus as necessary
	twi_acquire_or_release_bus(twi);
}


ISR(TWI_vect)
{
	twi_update(&twiState_g, TW_STATUS);
}

void hal_twi_init(Addr local_addr, TWIRecvSlot *trs)
{
	/* initialize our local state */
	twiState_g.trs = trs;
	twiState_g.have_bus = 0;
	twiState_g.out_pkt = NULL;
	twiState_g.out_len = 0;
	twiState_g.sendCallbackAct.func = (ActivationFunc) doSendCallback;
	twiState_g.sendCallbackAct.twi = &twiState_g;

	twiState_g.in_n = -1;
	twiState_g.recvCallbackAct.func = (ActivationFunc) doRecvCallback;
	twiState_g.recvCallbackAct.twi = &twiState_g;
	twiState_g.initted = TRUE;

	/* set 100khz (assuming 8mhz local clock!  fix me...) */
	TWBR = 32;

	/* configure the local address */
	TWAR = local_addr << 1;

	/* set the mask to be all 0, meaning all bits of the address are
	 * significant */
	TWAMR = 0;

	reg_set(&TWCR, TWEN); // enable twi
	reg_set(&TWCR, TWIE); // enable twi interrupts
	reg_set(&TWCR, TWEA); // ack incoming transmissions

	/* enable interrupts */
	sei();
}

void hal_twi_send(Addr dest_addr, char *data, uint8_t len, 
				  TWISendDoneFunc sendDoneCB, void *sendDoneCBData)
{
	hal_start_atomic();
	assert(twiState_g.out_pkt == NULL);

	twiState_g.out_pkt = data;
	twiState_g.out_destaddr = dest_addr;
	twiState_g.out_len = len;
	twiState_g.out_n = 0;
	twiState_g.sendDoneCB = sendDoneCB;
	twiState_g.sendDoneCBData = sendDoneCBData;

	twi_acquire_or_release_bus(&twiState_g);
	hal_end_atomic();
}
