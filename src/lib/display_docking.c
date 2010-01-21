#include "rocket.h"
#include "display_docking.h"
#include "rasters.h"
#include "region.h"
#include "sound.h"
#include "hpam.h"

UIEventDisposition ddock_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void ddock_update_once(DDockAct *act);
void ddock_update(DDockAct *act);
uint32_t ddock_compute_dx(int yc, int r, int y);
void ddock_paint_axes(DDockAct *act);
void ddock_thruster_update(DockThrusterUpdate *dtu, ThrusterPayload *tp);
void dd_bump(DDockAct *act, uint8_t thruster_bit, uint32_t xscale, uint32_t yscale);
void ddock_hide(DDockAct *dd);
void ddock_show(DDockAct *dd);

#define DD_MAX_POS 20
#define DD_MIN_RADIUS 3
#define DD_MAX_RADIUS 14
#define DD_SPEED_LIMIT 3
#define DD_GROW_SPEED 1
#define DD_GROW_SPEED_SCALE 2
#define DD_THRUSTER_DOWNSCALE	4
#define DD_TOLERANCE	2

void ddock_init(DDockAct *act, Screen4 *s4, uint8_t auxboard_base, AudioClient *audioClient, Booster *booster, JoystickState_t *joystick)
{
	act->func = (ActivationFunc) ddock_update;
	//init_screen4(&act->s4, b0);
	act->s4 = s4;

	act->audioClient = audioClient;

	act->handler.func = (UIEventHandlerFunc) ddock_event_handler;
	act->handler.act = act;

	act->thrusterUpdate.func = (ThrusterUpdateFunc) ddock_thruster_update;
	act->thrusterUpdate.act = act;

	act->booster = booster;
	act->joystick = joystick;

	act->focused = FALSE;

	drift_anim_init(&act->xd, 10, 0, -DD_MAX_POS, DD_MAX_POS, DD_SPEED_LIMIT);
	drift_anim_init(&act->yd, 10, 0, -DD_MAX_POS, DD_MAX_POS, DD_SPEED_LIMIT);
	drift_anim_init(&act->rd, 10, DD_MIN_RADIUS, DD_MIN_RADIUS, DD_MAX_RADIUS, DD_SPEED_LIMIT);
	act->last_impulse_time = 0;

	act->thrusterPayload.thruster_bits = 0;
	ddock_reset(act);

	board_buffer_init(&act->auxboards[0] DBG_BBUF_LABEL("dock"));
	board_buffer_init(&act->auxboards[1] DBG_BBUF_LABEL("dock"));
	board_buffer_push(&act->auxboards[0], auxboard_base+0);
	board_buffer_push(&act->auxboards[1], auxboard_base+1);
	act->auxboard_base = auxboard_base;

	ddock_hide(act);

	schedule_us(1, (Activation*) act);
}

void ddock_hide(DDockAct *dd)
{
	s4_hide(dd->s4);
	board_buffer_pop(&dd->auxboards[0]);
	board_buffer_pop(&dd->auxboards[1]);
	booster_set(dd->booster, FALSE);
}

void ddock_show(DDockAct *dd)
{
	s4_show(dd->s4);
	board_buffer_push(&dd->auxboards[0], dd->auxboard_base+0);
	board_buffer_push(&dd->auxboards[1], dd->auxboard_base+1);
}

void ddock_init_axis(DriftAnim *da)
{
	da_random_impulse(da);
	da_set_random_value(da);
	// be sure velocity isn't pushing away from center
	if ((da->base>0) == (da->velocity>0))
	{
		da->velocity = -da->velocity;
	}
}

void ddock_reset(DDockAct *dd)
{
	ddock_init_axis(&dd->xd);
	//LOGF((logfp, "ddock_reset: xd_b %d xd_v %d\n", dd->xd.base, dd->xd.velocity));
	ddock_init_axis(&dd->yd);
	da_set_value(&dd->rd, DD_MIN_RADIUS);
	da_set_velocity_scaled(&dd->rd, DD_GROW_SPEED, DD_GROW_SPEED_SCALE);
	dd->docking_complete = FALSE;
}

#define IMPULSE_FREQUENCY_US 5000000

void ddock_update(DDockAct *act)
{
	schedule_us(Exp2Time(17), (Activation*) act);

	ddock_update_once(act);

#if RANDOM_DRIFT
	if (!act->focused)
	{
		// standard animation
		Time t = clock_time_us();
		if (t - act->last_impulse_time > IMPULSE_FREQUENCY_US)
		{
			da_random_impulse(&act->xd);
			da_random_impulse(&act->yd);
			da_random_impulse(&act->rd);
			act->last_impulse_time = t;
		}
	}
#endif // RANDOM_DRIFT

	_da_update_base(&act->xd);
	_da_update_base(&act->yd);
	dd_bump(act, hpam_thruster_rear,   		0, -2);
	dd_bump(act, hpam_thruster_frontleft,   1,  1);
	dd_bump(act, hpam_thruster_frontright, -1,  1);
	//LOGF((logfp, "velocity %6d %6d\n", act->xd.velocity, act->yd.velocity));

	if (act->focused)
	{
		r_bool button_pushed = ((act->joystick->state & JOYSTICK_TRIGGER)!=0);
		booster_set(act->booster, button_pushed);
	}

	if (da_read(&act->rd) == DD_MAX_RADIUS)
	{
		int x = da_read(&act->xd);
		int y = da_read(&act->yd);
		r_bool success = ((-DD_TOLERANCE<=x && x<=DD_TOLERANCE)
						&& (-DD_TOLERANCE<=y && y<=DD_TOLERANCE));
		if (success && !act->docking_complete)
		{
			// TODO snap clanger solenoid
			ac_skip_to_clip(act->audioClient, sound_dock_clang, sound_silence);
			act->docking_complete = TRUE;
			LOGF((logfp, "docking success!\n"));
		}
	}
			
/*
	LOGF((logfp, "x %d y %d rad %d\n",
		da_read(&act->xd),
		da_read(&act->yd),
		da_read(&act->rd)));
*/
}

void dd_bump(DDockAct *act, HPAMIndex thruster_index, uint32_t xscale, uint32_t yscale)
{
	//LOGF((logfp, "dd_bump(hpam %x tb %x) %d\n", 1<<thruster_index, act->thrusterPayload.thruster_bits, act->thrusterPayload.thruster_bits & (1<<thruster_index)));
	if (act->thrusterPayload.thruster_bits & (1<<thruster_index))
	{
#if DD_INERTIA
		act->xd.velocity += (DD_SPEED_LIMIT*xscale) << (act->xd.expscale - DD_THRUSTER_DOWNSCALE);
		da_bound_velocity(&act->xd);

		act->yd.velocity += (DD_SPEED_LIMIT*yscale) << (act->yd.expscale - DD_THRUSTER_DOWNSCALE);
		da_bound_velocity(&act->yd);
#else // DD_INERTIA
		da_set_velocity(&act->xd, 0);
		act->xd.base += DD_SPEED_LIMIT*xscale << (act->xd.expscale - DD_THRUSTER_DOWNSCALE);

		da_set_velocity(&act->yd, 0);
		act->yd.base += DD_SPEED_LIMIT*yscale << (act->yd.expscale - DD_THRUSTER_DOWNSCALE);
#endif // DD_INERTIA
	}
}

#define I_NAN	 0x7fffffff
#define DD_SC	3

void ddock_show_complete(DDockAct *act)
{
	raster_clear_buffers(&act->s4->rrect);
	ascii_to_bitmap_str(act->s4->rrect.bbuf[1]->buffer, 8, "Docking");
	ascii_to_bitmap_str(act->s4->rrect.bbuf[2]->buffer, 8, "complete");
	raster_draw_buffers(&act->s4->rrect);
}

void dock_update_aux_display(DDockAct *act, int xc, int yc, int rad)
{
	char buf[9];
	strcpy(buf, "Range   ");
	int_to_string2(buf+5, 3, 0, DD_MAX_RADIUS - da_read(&act->rd));
	ascii_to_bitmap_str(act->auxboards[0].buffer, 8, buf);
	board_buffer_draw(&act->auxboards[0]);

	memset(buf, ' ', 8);
	int_to_string2(buf, 3, 0, xc);
	buf[3] = ' ';	// blast stupid string terminator
	int_to_string2(buf+5, 3, 0, yc);
	ascii_to_bitmap_str(act->auxboards[1].buffer, 8, buf);
	board_buffer_draw(&act->auxboards[1]);
}

void ddock_update_once(DDockAct *act)
{
	if (!act->focused)
	{
		// don't draw on borrowed s4 surface!
		return;
	}

	if (act->docking_complete)
	{
		ddock_show_complete(act);
		return;
	}

	int xc  = da_read_clip(&act->xd, 0, TRUE);
	int yc_s  = da_read_clip(&act->yd, DD_SC, TRUE);
	int rad = da_read_clip(&act->rd, DD_SC, FALSE);

	dock_update_aux_display(act, xc, yc_s>>DD_SC, rad);

	//LOGF((logfp, "ddock_update_once: %d %d %d\n", xc, act->xd.base, act->xd.velocity));

	raster_clear_buffers(&act->s4->rrect);

#if STARFIELD
	int star = 0;
	while (TRUE)
	{
		int ys = star / ((DD_MAX_POS<<DD_SC)*4);
		int xs = star - ys * ((DD_MAX_POS<<DD_SC)*4);
		int y = (ys >> DD_SC) - DD_MAX_POS*2;
		int x = (xs >> DD_SC) - DD_MAX_POS*2;
		LOGF((logfp, "star %d : %2d, %2d at %4d, %4d\n", star, x, y, x+xc, y+(yc_s>>DD_SC)));
		raster_paint_pixel(&act->s4->rrect,  x+xc, y+(yc_s>>DD_SC));
		star += 2269;
		if (y>DD_MAX_POS*2) { break; }
	}
#endif // STARFIELD

	// Here, y & x are in centered coordinates -- offset by MAX_*/2.
	int y;
	for (y=-MAX_Y/2; y<MAX_Y/2; y++)
	{
		int dx = ddock_compute_dx(yc_s, rad, y<<DD_SC) >> DD_SC;
		if (0<=dx && dx<DD_MAX_POS)
		{
			raster_paint_pixel(&act->s4->rrect, xc-dx   +CTR_X, y+CTR_Y);
			raster_paint_pixel(&act->s4->rrect, xc-dx+1 +CTR_X, y+CTR_Y);

#if STARFIELD
			// panit black in the middle of the target to blot out any
			// stars behind it.
			int x;
			for (x=xc-dx+2; x<xc+dx-1; x++)
			{
				raster_paint_pixel_v(&act->s4->rrect, x +CTR_X, y+CTR_Y, FALSE);
			}
#endif // STARFIELD
			
			raster_paint_pixel(&act->s4->rrect, xc+dx-1 +CTR_X, y+CTR_Y);
			raster_paint_pixel(&act->s4->rrect, xc+dx   +CTR_X, y+CTR_Y);
		}
	}

	ddock_paint_axes(act);

	raster_draw_buffers(&act->s4->rrect);
}

void ddock_paint_axes(DDockAct *act)
{
	int y;
	for (y=0; y<MAX_Y; y++)
	{
		// always paint a 2-pixel region to be sure we spill some ink
		raster_paint_pixel(&act->s4->rrect, MAX_X/2, y);
	}

	int x;
	for (x=0; x<MAX_X; x++)
	{
		raster_paint_pixel(&act->s4->rrect, x, MAX_Y/2);
	}
}

uint32_t ddock_compute_dx(int yc, int r, int y)
{
	int dy = (y - yc);
	int dx2 = r*r - dy*dy;
	if (dx2 < 0)
		return I_NAN;
	return isqrt(dx2);
}

UIEventDisposition ddock_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	DDockAct *act = ((DDockHandler *) raw_handler)->act;

	UIEventDisposition result = uied_accepted;
	switch (evt)
	{
#if RANDOM_DRIFT
		case '2':
			da_set_value(&act->yd, da_read(&act->yd)+1); break;
		case '8':
			da_set_value(&act->yd, da_read(&act->yd)-1); break;
		case '4':
			da_set_value(&act->xd, da_read(&act->xd)-1); break;
		case '6':
			da_set_value(&act->xd, da_read(&act->xd)+1); break;
		case 'p':
		case 'a':
			da_set_value(&act->rd, da_read(&act->rd)+1); break;
		case 's':
		case 'b':
			da_set_value(&act->rd, da_read(&act->rd)-1); break;
		case '0':
			da_set_value(&act->xd, 0);
			da_set_value(&act->yd, 0);
			break;
#endif // RANDOM_DRIFT
		case uie_focus:
			if (!act->focused)
			{
				ddock_reset(act);
				ddock_update_once(act);
				ddock_show(act);
				act->focused = TRUE;
			}
			break;
		case uie_escape:
			if (act->focused)
			{
				ddock_hide(act);
				act->focused = FALSE;
			}
			result = uied_blur;
			break;
	}

	return result;
}

void ddock_thruster_update(DockThrusterUpdate *dtu, ThrusterPayload *tp)
{
	//LOGF((logfp, "ddock_thruster_update %02x\n", tp->thruster_bits));
	dtu->act->thrusterPayload.thruster_bits = tp->thruster_bits;
}
