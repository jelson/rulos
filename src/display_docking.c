#include "rocket.h"
#include "display_docking.h"

UIEventDisposition ddock_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void ddock_update_once(DDockAct *act);
void ddock_update(DDockAct *act);
uint32_t ddock_compute_dx(int yc, int r, int y);
void ddock_draw_buffers(DDockAct *act);
void ddock_clear_buffer(DDockAct *act);
void ddock_paint_axes(DDockAct *act);
void ddock_paint_pixel(DDockAct *dock, int x, int y);

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
	RectRegion rr = {act->btable, DOCK_HEIGHT, 0, 7};
	focus_register(focus, (UIEventHandler*) &act->handler, rr, "docking");

	drift_anim_init(&act->xd, 10, 0, -14, 14, 5);
	drift_anim_init(&act->yd, 10, 0, -15, 15, 5);
	drift_anim_init(&act->rd, 10, 7, 7, 13, 5);
	act->last_impulse_time = 0;

	schedule(1, (Activation*) act);
}

#define IMPULSE_FREQUENCY_MS 5000

void ddock_update(DDockAct *act)
{
	ddock_update_once(act);

	if (!act->focused)
	{
		// standard animation
		uint32_t t = clock_time();
		if (t - act->last_impulse_time > IMPULSE_FREQUENCY_MS)
		{
			da_random_impulse(&act->xd);
			da_random_impulse(&act->yd);
			da_random_impulse(&act->rd);
			act->last_impulse_time = t;
		}
	}
	schedule(256, (Activation*) act);
}

#define I_NAN	 0x7fffffff

void ddock_update_once(DDockAct *act)
{
	int xc = da_read(&act->xd);
	int yc = da_read(&act->yd);
	int rad = da_read(&act->rd);

	ddock_clear_buffer(act);

	ddock_paint_axes(act);

	// Here, y & x are in centered coordinates -- offset by MAX_*/2.
	int y;
	for (y=-MAX_Y/2; y<MAX_Y/2; y++)
	{
		int dx = ddock_compute_dx(yc, rad, y+0);
		if (dx!=I_NAN)
		{
			ddock_paint_pixel(act, xc-dx   +CTR_X, y+CTR_Y);
			ddock_paint_pixel(act, xc-dx+1 +CTR_X, y+CTR_Y);
			ddock_paint_pixel(act, xc+dx   +CTR_X, y+CTR_Y);
			ddock_paint_pixel(act, xc+dx-1 +CTR_X, y+CTR_Y);
		}
	}

	ddock_draw_buffers(act);
}

void ddock_draw_buffers(DDockAct *act)
{
	int row;
	for (row=0; row<DOCK_HEIGHT; row++)
	{
		board_buffer_draw(&act->bbuf[row]);
	}
}

void ddock_clear_buffer(DDockAct *act)
{
	int row;
	for (row=0; row<DOCK_HEIGHT; row++)
	{
		memset(act->bbuf[row].buffer, 0, NUM_DIGITS);
	}
}

void ddock_paint_axes(DDockAct *act)
{
	int y;
	for (y=0; y<MAX_Y; y++)
	{
		// always paint a 2-pixel region to be sure we spill some ink
		ddock_paint_pixel(act, MAX_X/2, y);
	}

	int x;
	for (x=0; x<MAX_X; x++)
	{
		ddock_paint_pixel(act, x, MAX_Y/2);
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

static SSBitmap _sevseg_pixel_mask[6][4] = {
	{ 0b00000000, 0b01000000, 0b00000000, 0b00000000 },
	{ 0b00000010, 0b00000000, 0b00100000, 0b00000000 },
	{ 0b00000000, 0b00000001, 0b00000000, 0b00000000 },
	{ 0b00000100, 0b00000000, 0b00010000, 0b00000000 },
	{ 0b00000000, 0b00001000, 0b00000000, 0b10000000 },
	{ 0b00000000, 0b00000000, 0b00000000, 0b00000000 }
	};

void ddock_paint_pixel(DDockAct *dock, int x, int y)
{
	int maj_x = x/4;
	int min_x = x-(maj_x*4);
	int maj_y = y/6;
	int min_y = y-(maj_y*6);

	if (maj_y<0 || maj_y >= DOCK_HEIGHT
		|| maj_x<0 || maj_x >= NUM_DIGITS) { return; }

	dock->bbuf[maj_y].buffer[maj_x] |= _sevseg_pixel_mask[min_y][min_x];
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
