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
#include "periph/rocket/sound.h"

typedef struct {
	SoundToken token;
} SoundCmd;

typedef struct {
	uint8_t stream_id;
	r_bool skip;
	SoundCmd skip_cmd;
	SoundCmd loop_cmd;
} AudioRequestMessage;

typedef struct {
	uint8_t stream_id;
	uint8_t mlvolume;
} AudioVolumeMessage;

typedef struct {
	int8_t advance;	// +1: skip forward  -1: skip backward
} MusicControlMessage;