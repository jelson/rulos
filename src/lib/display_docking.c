#include "rocket.h"
#include "display_docking.h"
#include "rasters.h"

UIEventDisposition ddock_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void ddock_update_once(DDockAct *act);
void ddock_update(DDockAct *act);
uint32_t ddock_compute_dx(int yc, int r, int y);
void ddock_paint_axes(DDockAct *act);

void ddock_init(DDockAct *act, uint8_t b0, FocusManager *focus)
{
	act->func = (ActivationFunc) ddock_update;
	int i;
	for (i=0; i<DOCK_HEIGHT; i++)
	{
		board_buffer_init(&act->bbuf[i]);
		board_buffer_push(&act->bbuf[i], b0+i);
		act->btable[i] = &act->bbuf[i];
	}
	act->handler.func = (UIEventHandlerFunc) ddock_event_handler;
	act->handler.act = act;

	act->focused = FALSE;
	act->rrect.bbuf = act->btable;
	act->rrect.ylen = DOCK_HEIGHT;
	act->rrect.x = 0;
	act->rrect.xlen = 8;
	focus_register(focus, (UIEventHandler*) &act->handler, act->rrect, "docking");

	drift_anim_init(&act->xd, 10, 0, -14, 14, 10);
	drift_anim_init(&act->yd, 10, 0, -15, 15, 10);
	drift_anim_init(&act->rd, 10, 7, 7, 13, 10);
	act->last_impulse_time = 0;

	schedule_us(1, (Activation*) act);
}

#define IMPULSE_FREQUENCY_US 5000000

void ddock_update(DDockAct *act)
{
	ddock_update_once(act);

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
	schedule_us(Exp2Time(18), (Activation*) act);
}

#define I_NAN	 0x7fffffff

void ddock_update_once(DDockAct *act)
{
	int xc = da_read(&act->xd);
	int yc = da_read(&act->yd);
	int rad = da_read(&act->rd);

	raster_clear_buffers(&act->rrect);

	ddock_paint_axes(act);

	// Here, y & x are in centered coordinates -- offset by MAX_*/2.
	int y;
	for (y=-MAX_Y/2; y<MAX_Y/2; y++)
	{
		int dx = ddock_compute_dx(yc, rad, y+0);
		if (dx!=I_NAN)
		{
			raster_paint_pixel(&act->rrect, xc-dx   +CTR_X, y+CTR_Y);
			raster_paint_pixel(&act->rrect, xc-dx+1 +CTR_X, y+CTR_Y);
			raster_paint_pixel(&act->rrect, xc+dx   +CTR_X, y+CTR_Y);
			raster_paint_pixel(&act->rrect, xc+dx-1 +CTR_X, y+CTR_Y);
		}
	}

	raster_draw_buffers(&act->rrect);
}

void ddock_paint_axes(DDockAct *act)
{
	int y;
	for (y=0; y<MAX_Y; y++)
	{
		// always paint a 2-pixel region to be sure we spill some ink
		raster_paint_pixel(&act->rrect, MAX_X/2, y);
	}

	int x;
	for (x=0; x<MAX_X; x++)
	{
		raster_paint_pixel(&act->rrect, x, MAX_Y/2);
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
		case uie_focus:
			act->focused = TRUE;
			da_set_velocity(&act->xd, 0);
			da_set_velocity(&act->yd, 0);
			da_set_velocity(&act->rd, 0);
			break;
		case uie_escape:
			act->focused = FALSE;
			result = uied_blur;
			break;
	}

	return result;
}
