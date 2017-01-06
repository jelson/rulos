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

#include <stdlib.h>

#include "clock.h"
#include "uart_net_media.h"

void audioled_set(r_bool red, r_bool yellow);

void _um_send(MediaStateIfc *media,
	Addr dest_addr, char *data, uint8_t len, 
	MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);

void _um_recv_handler(struct s_UartHandler *u, char c);
r_bool _um_send_handler(struct s_UartHandler *u, char *c /*OUT*/);

MediaStateIfc *uart_media_init(UartMedia *um, MediaRecvSlot *mrs, uint8_t uart_id)
{
	um->uart_handler.send = _um_send_handler;
	um->uart_handler.recv = _um_recv_handler;
	um->uart_media_ptr.media.send = _um_send;
	um->uart_media_ptr.uart_media = um;
	um->send_which = US_none;

	um->mrs = mrs;
	um->recv_state = UR_sync0;

	hal_uart_init(&um->uart_handler, 38400, TRUE, uart_id);
	return &um->uart_media_ptr.media;
}

void _um_send_complete(UartMediaSendingPayload *usp)
{
	usp->data = NULL;
	if (usp->sendDoneCB!=NULL)
	{
		usp->sendDoneCB(usp->sendDoneCBData);
	}
}

r_bool _um_send_handler(struct s_UartHandler *u, char *c /*OUT*/)
{
	UartMedia *um = (UartMedia *) u;
	if (um->send_which == US_packet)
	{
		(*c) = um->sending_payload.data[um->send_dataidx];
		um->send_dataidx+=1;
		if (um->send_dataidx == um->sending_payload.len)
		{
			um->send_which = US_none;
			schedule_now((ActivationFuncPtr) _um_send_complete, &um->sending_payload);
		}
		return TRUE;
	}
	else if (um->send_which == US_preamble)
	{
		(*c) = ((char*) &um->sending_preamble)[um->send_dataidx];
		um->send_dataidx+=1;
		if (um->send_dataidx == sizeof(um->sending_preamble))
		{
			um->send_which = US_packet;
			um->send_dataidx = 0;
		}
		return TRUE;
	}
	else if (um->send_which == US_none)
	{
		return FALSE;
	}
	else
	{
		return FALSE;
	}
}

void _um_send(MediaStateIfc *media,
	Addr dest_addr, char *data, uint8_t len, 
	MediaSendDoneFunc sendDoneCB, void *sendDoneCBData)
{
	
	UartMedia *um = ((UartMediaPtr*) media)->uart_media;
	if (um->send_which != US_none)
	{
		return;
	}
	if (um->sending_payload.data!=NULL)
	{
		return;
	}
	um->sending_preamble.p0 = UM_PREAMBLE0;
	um->sending_preamble.p1 = UM_PREAMBLE1;
	um->sending_preamble.dest_addr = dest_addr;
	um->sending_preamble.len = len;
	um->sending_payload.data = data;
	um->sending_payload.len = len;
	um->sending_payload.sendDoneCB = sendDoneCB;
	um->sending_payload.sendDoneCBData = sendDoneCBData;
	um->send_dataidx = 0;
	um->send_which = US_preamble;
	hal_uart_start_send(&um->uart_handler);
}

void _um_recv_handler(struct s_UartHandler *u, char c)
{
	UartMedia *um = (UartMedia*) u;
	if (um->mrs->occupied_len > 0)
	{
		um->recv_state = UR_sync0;
		return;
	}

	switch (um->recv_state)
	{
	case UR_sync0:
		if (c==UM_PREAMBLE0) {
			um->recv_state = UR_sync1;
		}
		break;
	case UR_sync1:
		if (c==UM_PREAMBLE1) {
			um->recv_state = UR_addr;
		} else {
			um->recv_state = UR_sync0;
		}
		break;
	case UR_addr:
		um->recv_addr = c;
		um->recv_state = UR_len;
		break;
	case UR_len:
		um->recv_len = c;
		um->recv_dataidx = 0;
		um->recv_state = UR_payload;
		break;
	case UR_payload:
		if (um->recv_dataidx < um->mrs->capacity)
		{
			um->mrs->data[um->recv_dataidx] = c;
		}
		um->recv_dataidx += 1;
		if (um->recv_dataidx == um->recv_len)
		{
			// end of packet.
			if (um->recv_len <= um->mrs->capacity)
			{
				um->mrs->occupied_len = um->recv_len;
				audioled_set(1, 0);
				(um->mrs->func)(um->mrs, um->recv_len);
				audioled_set(1, 1);
			}
			// else:
			// received a too-long packet. Wanted to run whole
			// state machine anyway (to avoid accidentally resyncing
			// mid-packet when we have a valid length field),
			// but can't deliver a broken packet.
			um->recv_state = UR_sync0;
		}
		break;
	default:
		assert(FALSE);
	}
}
