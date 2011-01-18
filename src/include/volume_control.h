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

#ifndef _VOLUME_CONTROL_H
#define _VOLUME_CONTROL_H

#include <rocket.h>
#include <input_controller.h>
#include <potsticker.h>
#include <audio_client.h>

#define DISPLAY_VOLUME_ADJUSTMENTS	0
	// TODO drawing volume on the display burns 21 bytes of bss;
	// here's where to reclaim them.

typedef struct
{
	uint8_t mlvolume;
} SetVolumePayload;

struct s_volume_control;

typedef struct
{
	InputInjectorIfc iii;
	struct s_volume_control *vc;
} VolumeControlInjector;

typedef struct s_volume_control
{
#if DISPLAY_VOLUME_ADJUSTMENTS
	Activation act;
#endif // DISPLAY_VOLUME_ADJUSTMENTS
	VolumeControlInjector injector;
	PotSticker potsticker;
	AudioClient *ac;

	uint8_t cur_vol;

#if DISPLAY_VOLUME_ADJUSTMENTS
	Time lastTouch;
	r_bool visible;
	uint8_t boardnum;
	BoardBuffer bbuf;
#endif // DISPLAY_VOLUME_ADJUSTMENTS
} VolumeControl;

void volume_control_init(VolumeControl *vc, AudioClient *ac, uint8_t adc_channel, uint8_t boardnum);

static inline uint8_t volume_get_mlvolume(VolumeControl *vc)
{
	return vc->cur_vol;
}


#endif // _VOLUME_CONTROL_H
