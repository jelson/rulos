#ifndef _uart_net_media_preamble_h
#define _uart_net_media_preamble_h

#include "core/media.h"

#define UM_PREAMBLE0	0xdc
#define UM_PREAMBLE1	0x95

typedef struct
{
	char p0;
	char p1;
	Addr dest_addr;
	uint8_t len;
} UartPreamble;

#endif // _uart_net_media_preamble_h
