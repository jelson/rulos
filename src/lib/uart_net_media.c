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

#include "uart_net_media.h"

void _um_send(MediaStateIfc *media,
	Addr dest_addr, char *data, uint8_t len, 
	MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);
void _um_send_payload(void *v_um);
void _um_payload_sent(void *v_um);

MediaStateIfc *uart_media_init(UartMedia *um, MediaRecvSlot *mrs)
{
	uint32_t myubrr = hardware_f_cpu/16/38400-1;
	uart_init(RULOS_UART0, myubrr, TRUE);
	um->media.send = _um_send;
	um->drain.act.func = _um_drain_queue;
	um->drain.act.um = um;
	um->recvQueue = uart_recvq(RULOS_UART0);

	schedule_us(1, &sca->act);
}

// TODO: clean up uart hal layer to just provide a raw interrupt-time
// callback. The queue is overkill here.
void _um_drain_queue(Activation *act)
{
	UartMedia *um = ((UartDrainAct *)act)->um;
	while (CharQueue_pop(sca->recvQueue->q, &rcv_chr)) {
	}
	schedule_us(120, &sca->act);
}

void _um_send(MediaStateIfc *media,
	Addr dest_addr, char *data, uint8_t len, 
	MediaSendDoneFunc sendDoneCB, void *sendDoneCBData)
{
	UartMedia *um = (UartMedia*) media;
	assert(!uart_busy(RULOS_UART0));
	assert(um->sending_payload.data==NULL);
	um->sending_preamble.p0 = UM_PREAMBLE0;
	um->sending_preamble.p1 = UM_PREAMBLE1;
	um->sending_preamble.dest_addr = dest_addr;
	um->sending_preamble.len = len;
	um->sending_payload.data = data;
	um->sending_payload.len = len;
	um->sending_payload.sendDoneCB = sendDoneCB;
	um->sending_payload.sendDoneCBData = sendDoneCBData;
	uart_send(RULOS_UART0, (char*) um->sending_preamble, sizeof(um->sending_preamble), _um_send_payload, um);
}

void _um_send_payload(void *v_um)
{
	UartMedia *um = (UartMedia *)v_um;
	assert(!uart_busy(RULOS_UART0));
	uart_send(RULOS_UART0,
		um->sending_payload.data,
		um->sending_payload.len,
		_um_payload_sent,
		um);
}
void _um_payload_sent(void *v_um)
{
	UartMedia *um = (UartMedia *)v_um;
	um->sending_payload.data = NULL;
	um->sending_payload.sendDoneCB(um->sending_payload.sendDoneCBData);
}


