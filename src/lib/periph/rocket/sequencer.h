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

/*
 * play a scripted storyboard.
 *
 * EG: launch:
 * 0. board 0 prompt: "Initiate launch sequence: Enter A476" (move focus
 * appropriately) 0. board 1: display entered string await input; reset schedule
 * 0. scroll board 0: "launch sequence initiated."
 * 0. clock panel 1: t-(10-now)
 * 10. scroll panel 0: blinking "LAUNCH"
 * 10. valve_open(VALVE_BOOSTER)
 * 10. sound_start(SOUND_LAUNCH_NOISE)
 * 20. valve_close(VALVE_BOOSTER)
 * 20. sound_start(SOUND_FLAME_OUT)
 * 20. scroll board 0: "launch complete. Orbit attained."
 */

#include "periph/audio/audio_server.h"
#include "periph/display_rtc/display_rtc.h"
#include "periph/hpam/hpam.h"
#include "periph/input_controller/focus.h"
#include "periph/rasters/rasters.h"
#include "periph/rocket/booster.h"
#include "periph/rocket/display_blinker.h"
#include "periph/rocket/display_scroll_msg.h"
#include "periph/rocket/display_thrusters.h"
#include "periph/rocket/lunar_distance.h"
#include "periph/rocket/numeric_input.h"
#include "periph/rocket/screen4.h"

typedef enum {
  launch_state_init = -1,
  launch_state_hidden = 0,
  launch_state_enter_code,
  launch_state_wrong_code,
  launch_state_countdown,
  launch_state_launching,
  launch_state_complete,
} LaunchState;

struct s_screen_blanker;

typedef struct s_launch {
  UIEventHandlerFunc func;

  LaunchState state;

  Screen4 *s4;
  DScrollMsgAct dscrlmsg;
  BoardBuffer code_label_bbuf;
  BoardBuffer code_value_bbuf;
  BoardBuffer textentry_bbuf;
  NumericInputAct textentry;
  RasterBigDigit bigDigit;

  DRTCAct *main_rtc;
  LunarDistance *lunar_distance;

  Time nextEventTimeout;

  Booster *booster;
  AudioClient *audioClient;
  HPAM *hpam;
  ThrusterState_t *thrusterState;
  uint16_t launch_code;
  char launch_code_str[45];

  // for the "thruster spinner" during the launch
  char thrusterSpinnerOn;
  Time thrusterSpinnerNextTimeout;
  Time thrusterSpinnerPeriod;
  int thrusterSpinnerNextThruster;

  struct s_screen_blanker *screenblanker;
} Launch;

void launch_init(Launch *launch, Screen4 *s4, Booster *booster, HPAM *hpam,
                 ThrusterState_t *thrusterState, AudioClient *audioClient,
                 struct s_screen_blanker *screenblanker);
