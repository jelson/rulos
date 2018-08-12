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

#ifndef _hpam_h
#define _hpam_h

#include "core/clock.h"
#include "core/network.h"
#include "core/util.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/rocket/thruster_protocol.h"

typedef struct {
	r_bool status;
	uint8_t rest_time_secs;
	Time expire_time;	// time when needs to rest, or when rest complete
	r_bool resting;
	uint8_t board;
	uint8_t digit;
	uint8_t segment;
} HPAMPort;

typedef enum {
	hpam_hobbs = 0,
	hpam_reserved = 1,		// future: clanger, hatch open solenoid
	hpam_lighting_flicker = 2,
	hpam_five_volts = 3,	// 5V channel, no longer used
	hpam_thruster_frontleft = 4,
	hpam_thruster_frontright = 5,
	hpam_thruster_rear = 6,
	hpam_booster = 7,
	hpam_end
} HPAMIndex;

// Future: gauges on a 12V HPAM.

typedef struct {
	HPAMPort hpam_ports[hpam_end];
	ThrusterUpdate *thrusterUpdates;
	BoardBuffer bbuf;
	ThrusterPayload thrusterPayload;
} HPAM;

void init_hpam(HPAM *hpam, uint8_t board0, ThrusterUpdate *thrusterUpdates);
void hpam_set_port(HPAM *hpam, HPAMIndex idx, r_bool status);
r_bool hpam_get_port(HPAM *hpam, HPAMIndex idx);

#endif // _hpam_h
