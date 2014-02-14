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

#include "rocket.h"
#include "hpam.h"
#include "joystick.h"

struct {
	char *name;
	HPAMIndex hpamIndex;
} hpams[] = {
	{"hobbs", hpam_hobbs},
	{"clanger", hpam_clanger},
	{"hatch", hpam_hatch_solenoid_reserved},
	{"lights", hpam_lighting_flicker},
	{"th_frLft", hpam_thruster_frontleft},
	{"th_frRgt", hpam_thruster_frontright},
	{"th_rear", hpam_thruster_rear},
	{"booster", hpam_booster},
	{NULL, 0},
};

char *idx_to_name(HPAMIndex idx)
{
	int i;
	for (i=0; i<hpam_end; i++)
	{
		if (hpams[i].hpamIndex==idx)
		{
			return hpams[i].name;
		}
	}
	return "none";
}

typedef struct {
	JoystickState_t js;
	int8_t dir;
	r_bool btn;
	uint8_t idx;
	ThrusterUpdate *tu;
	HPAM hpam;
	BoardBuffer idx_bbuf;
	BoardBuffer state_bbuf;
} HTAct;

void ht_update(HTAct *ht)
{
	schedule_us((Time) 10000, (ActivationFuncPtr) ht_update, ht);

#define DBG_BONK 0
#if DBG_BONK
	memset(ht->state_bbuf.buffer, 0, NUM_DIGITS);
	ht->state_bbuf.buffer[NUM_DIGITS-1] = ascii_to_bitmap('0' + ((clock_time_us()>>20) & 7));
	ht->state_bbuf.buffer[NUM_DIGITS-2] = ascii_to_bitmap('t');
	ht->state_bbuf.buffer[NUM_DIGITS-4] = ascii_to_bitmap(hal_read_joystick_button() ? '*' : '_');
	board_buffer_draw(&ht->state_bbuf);
	return;
#endif // DBG_BONK

	joystick_poll(&ht->js);
	int8_t new_dir =
		(ht->js.state & JOYSTICK_LEFT) ? -1 :
			(ht->js.state & JOYSTICK_RIGHT) ? +1 : 0;
	if (new_dir != ht->dir)
	{
		if (ht->dir==-1)
		{
			ht->idx = (ht->idx-1+hpam_end) % hpam_end;
		}
		else if (ht->dir==+1)
		{
			ht->idx = (ht->idx+1+hpam_end) % hpam_end;
		}
		ht->dir = new_dir;
	}
	r_bool new_btn = (ht->js.state & JOYSTICK_TRIGGER) != 0;
	if (new_btn != ht->btn)
	{
		if (!ht->btn)
		{
			hpam_set_port(&ht->hpam, ht->idx,
				!hpam_get_port(&ht->hpam, ht->idx));
		}
		ht->btn = new_btn;
	}

	memset(ht->idx_bbuf.buffer, 0, NUM_DIGITS);
	ascii_to_bitmap_str(ht->idx_bbuf.buffer, NUM_DIGITS, idx_to_name(ht->idx));
	board_buffer_draw(&ht->idx_bbuf);

	memset(ht->state_bbuf.buffer, 0, NUM_DIGITS);
	ascii_to_bitmap_str(ht->state_bbuf.buffer, NUM_DIGITS,
		hpam_get_port(&ht->hpam, ht->idx) ? "on" : "off");
	ht->state_bbuf.buffer[NUM_DIGITS-1] = ascii_to_bitmap('0' + ((clock_time_us()>>20) & 7));
	ht->state_bbuf.buffer[NUM_DIGITS-2] = ascii_to_bitmap('t');
	ht->state_bbuf.buffer[NUM_DIGITS-3] = ascii_to_bitmap(new_btn ? '-' : '_');
	ht->state_bbuf.buffer[NUM_DIGITS-4] = ascii_to_bitmap(hal_read_joystick_button() ? '*' : '_');
	board_buffer_draw(&ht->state_bbuf);
}

void ht_init(HTAct *ht)
{
	ht->js.x_adc_channel = 3;
	ht->js.y_adc_channel = 2;
	ht->dir = 0;
	ht->btn = FALSE;
	ht->idx = 0;
	ht->tu = NULL;
	init_hpam(&ht->hpam, 7, ht->tu);
	joystick_init(&ht->js);
	board_buffer_init(&ht->idx_bbuf);
	board_buffer_push(&ht->idx_bbuf, 3);
	board_buffer_init(&ht->state_bbuf);
	board_buffer_push(&ht->state_bbuf, 4);
	schedule_us((Time) 10000, (ActivationFuncPtr) ht_update, ht);
}


int main()
{
	hal_init();
	hal_init_rocketpanel(bc_rocket0);
	init_clock(10000, TIMER1);
	board_buffer_module_init();
	CpumonAct cpumon;
	cpumon_init(&cpumon);

	HTAct ht;
	ht_init(&ht);

	cpumon_main_loop();

	return 0;
}
