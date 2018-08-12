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

#pragma once

#include "core/rulos.h"

// 5 -> known choppy


// 6 -> known choppy
//  11624	    128	   1125	  12877	   324d	audioboard.atmega328p.CUSTOM.elf
// 7 -> known good
//  11624	    128	   1317	  13069	   330d	audioboard.atmega328p.CUSTOM.elf
#define AO_BUFLENLG2	(7)
#define AO_BUFLEN		(((uint16_t)1)<<AO_BUFLENLG2)
#define AO_BUFMASK		((((uint16_t)1)<<AO_BUFLENLG2)-1)
#define AO_NUMBUFS		2

typedef struct
{
	uint8_t buffers[AO_NUMBUFS][AO_BUFLEN];
	uint8_t sample_index;
	uint8_t play_buffer;

	uint8_t fill_buffer;	// the one fill_act should populate when called.
	ActivationRecord fill_act;
} AudioOut;

void init_audio_out(AudioOut *ao, uint8_t timer_id, ActivationFuncPtr fill_func, void *fill_data);
