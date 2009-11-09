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
	lpc_blank,
	lpc_p0scroller,
	lpc_p0blinker,
	lpc_p1timer,
	lpc_p1textentry,
} LaunchPanelConfig;

typedef enum {
	launch_state_ready,
	launch_state_enter_code,
	launch_state_wrong_code,
	launch_state_countdown,
	launch_state_launching,
	launch_state_complete,
} LaunchState;

typedef enum {
	sequencer_timeout = 0x90,
} SequencerEvent;

struct s_launch;

typedef struct {
	ActivationFunc func;
	struct s_launch *launch;
} LaunchClockAct;

typedef struct s_launch {
	UIEventHandlerFunc func;
	uint8_t board0;
	uint8_t state;
	uint8_t p0_config;
	uint8_t p1_config;
	Time state_start_time;
	uint8_t timer_expired;
	Time timer_deadline;
	DScrollMsgAct p0_scroller;
	DBlinker p0_blinker;
	DRTCAct p1_timer;
	BoardBuffer textentry_bbuf;
	NumericInputAct p1_textentry;
	BoardBuffer *bbufary[2];	// for focus target. Tricky, because we swap them out. :v(
	LaunchClockAct clock_act;
	DRTCAct *main_rtc;
	LunarDistance *lunar_distance;
} Launch;

void launch_init(Launch *launch, uint8_t board0, FocusManager *fa);

#endif // _sequencer_h
