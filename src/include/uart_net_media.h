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


#ifndef _UART_NET_MEDIA_H
#define _UART_NET_MEDIA_H

#include "rocket.h"
#include "media.h"

#define UM_PREAMBLE0	0xdc
#define UM_PREAMBLE1	0x95

struct s_UartMedia;

typedef struct
{
	char p0;
	char p1;
	Addr dest_addr;
	uint8_t len;
} UartPreamble;

typedef struct
{
	char *data;
	uint8_t len;
	MediaSendDoneFunc sendDoneCB;
	void *sendDoneCBData;
} UartSendingPacket;

typedef struct
{
	Activation act;
	struct s_UartMedia *um;
} UartDrainAct;

typedef struct s_UartMedia
{
	MediaStateIfc media;
	UartDrainAct drain;
	UartQueue_t *recvQueue;
	UartPreamble sending_preamble;
	UartSendingPacket sending_payload;
} UartMedia;


// Strategy: mimic TWI interface, with uart media underneath.
// (Think PPP.)
// Then another 'multihost' layer will multiplex between the media
// on the audio board, and maybe even do packet forwarding. Tee hee.

MediaStateIfc *uart_media_init(UartMedia *um, MediaRecvSlot *mrs);

#endif // _UART_NET_MEDIA_H
