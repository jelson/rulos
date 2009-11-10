#ifndef __sequencer_h__
#define __sequencer_h__

/*
 * play a scripted storyboard.
 *
 * EG: launch:
 * 0. board 0 prompt: "Initiate launch sequence: Enter A476" (move focus appropriately)
 * 0. board 1: display entered string
 * await input; reset schedule
 * 0. scroll board 0: "launch sequence initiated."
 * 0. clock panel 1: t-(10-now)
 * 10. scroll panel 0: blinking "LAUNCH"
 * 10. valve_open(VALVE_BOOSTER)
 * 10. sound_start(SOUND_LAUNCH_NOISE)
 * 20. valve_close(VALVE_BOOSTER)
 * 20. sound_start(SOUND_FLAME_OUT)
 * 20. scroll board 0: "launch complete. Orbit attained."
 */

#include "display_scroll_msg.h"
#include "display_rtc.h"
#include "display_blinker.h"
#include "numeric_input.h"
#include "focus.h"
#include "lunar_distance.h"

typedef enum {
	launch_state_ready,
	launch_state_enter_code,
	launch_state_wrong_code,
	launch_state_countdown,
	launch_state_launching,
	launch_state_complete,
} LaunchState;

struct s_launch;

typedef struct {
	ActivationFunc func;
	struct s_launch *launch;
} LaunchClockAct;

#define LAUNCH_HEIGHT 4

typedef struct s_launch {
	UIEventHandlerFunc func;
	LaunchClockAct clock_act;
	DRTCAct *main_rtc;
	LunarDistance *lunar_distance;

	uint8_t board0;
	BoardBuffer bbuf[LAUNCH_HEIGHT];
	BoardBuffer *btable[LAUNCH_HEIGHT];
	RectRegion rrect;

	BoardBuffer textentry_bbuf;
	NumericInputAct textentry;
} Launch;

void launch_init(Launch *launch, uint8_t board0);

#endif // _sequencer_h
