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

#include "periph/audio/audio_client.h"
#include "periph/input_controller/input_controller.h"
#include "periph/rocket/potsticker.h"
#include "periph/rocket/rocket.h"

#define VOL_UP_KEY 't'
#define VOL_DN_KEY 'u'
//#define VOL_UP_KEY '9'
//#define VOL_DN_KEY '7'

#define DISPLAY_VOLUME_ADJUSTMENTS 1

// TODO drawing volume on the display burns 21 bytes of bss;
// here's where to reclaim them.

typedef struct {
  uint8_t mlvolume;
} SetVolumePayload;

struct s_volume_control;

typedef struct {
  InputInjectorIfc iii;
  struct s_volume_control *vc;
} VolumeControlInjector;

typedef struct s_volume_control {
#if DISPLAY_VOLUME_ADJUSTMENTS
#endif  // DISPLAY_VOLUME_ADJUSTMENTS
  VolumeControlInjector injector;
  PotSticker potsticker;
  AudioClient *ac;

  uint8_t cur_vol;

#if DISPLAY_VOLUME_ADJUSTMENTS
  Time lastTouch;
  r_bool visible;
  uint8_t boardnum;
  BoardBuffer bbuf;
#endif  // DISPLAY_VOLUME_ADJUSTMENTS
} VolumeControl;

void volume_control_init(VolumeControl *vc, AudioClient *ac,
                         uint8_t adc_channel, uint8_t boardnum);

static inline uint8_t volume_get_mlvolume(VolumeControl *vc) {
  return vc->cur_vol;
}
