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

#ifndef _network_ports_h
#define _network_ports_h

// Addresses
#define ROCKET_ADDR				(0x01)
#define ROCKET1_ADDR			(0x02)
#define AUDIO_ADDR				(0x03)

// Ports
#define THRUSTER_PORT			(0x11)
#define REMOTE_BBUF_PORT		(0x12)
#define REMOTE_KEYBOARD_PORT	(0x13)
#define REMOTE_SUBFOCUS_PORT0	(0x14)	/* remote_keyboard protocol */
#define AUDIO_PORT				(0x15)
#define SCREENBLANKER_PORT		(0x16)
#define UARTNETWORKTEST_PORT	(0x17)

#endif // _network_ports_h
