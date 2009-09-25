#include <inttypes.h>

#include "display_controller.h"
#include "display_docking.h"
#include "util.h"
#include "random.h"
#include "focus.h"

UIEventDisposition ddock_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void ddock_update_once(DDockAct *act);
void ddock_update(DDockAct *act);
int ddock_compute_dx(int yc, int r, int y);
void ddock_paint_hrow(SSBitmap *bm, int y, int x0, int x1, SSBitmap mask);

void ddock_init(DDockAct *act, uint8_t b0, FocusAct *focus)
{
	act->func = (ActivationFunc) ddock_update;
	int i;
	for (i=0; i<DOCK_HEIGHT; i++)
	{
		board_buffer_init(&act->bbuf[i]);
		board_buffer_push(&act->bbuf[i], b0+i);
	}
	act->handler.func = (UIEventHandlerFunc) ddock_event_handler;
	act->handler.act = act;

	act->focused = FALSE;
	DisplayRect rect = {b0, b0+DOCK_HEIGHT-1, 0, 7};
	focus_register(focus, (UIEventHandler*) &act->handler, rect);

	drift_anim_init(&act->xd, 10, 0, -14, 14, 5);
	drift_anim_init(&act->yd, 10, 0, -15, 15, 5);
	drift_anim_init(&act->rd, 10, 7, 7, 13, 5);
	act->last_impulse_time = 0;

	schedule(0, (Activation*) act);
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
	int row;
	for (row=0; row<DOCK_HEIGHT; row++)
	{
		SSBitmap *bm = act->bbuf[row].buffer;
		int i;

		// draw axes
		SSBitmap bg = (row==(DOCK_HEIGHT/2)) ? 0b1000000 : 0;
		for (i=0; i<NUM_DIGITS; i++)	// zero buffer
		{
			bm[i] = bg;
		}
		bm[NUM_DIGITS/2] |= 0b0110000;

		int y = DOCK_HEIGHT*7/2 - (row*7);
		int dx;

		dx = ddock_compute_dx(yc, rad, y+0);
		if (dx!=I_NAN)
			ddock_paint_hrow(bm, y+0, xc-dx+1, xc+dx+1, 0b0001000);

		dx = ddock_compute_dx(yc, rad, y+1);
		if (dx!=I_NAN)
		{
			ddock_paint_hrow(bm, y+1, xc-dx+0, xc+dx+0, 0b0010000);
			ddock_paint_hrow(bm, y+1, xc-dx+2, xc+dx+2, 0b0000100);
		}
		dx = ddock_compute_dx(yc, rad, y+2);
		if (dx!=I_NAN)
			ddock_paint_hrow(bm, y+2, xc-dx+1, xc+dx+1, 0b0000001);

		dx = ddock_compute_dx(yc, rad, y+3);
		if (dx!=I_NAN)
		{
			ddock_paint_hrow(bm, y+3, xc-dx+0, xc+dx+0, 0b0100000);
			ddock_paint_hrow(bm, y+3, xc-dx+2, xc+dx+2, 0b0000010);
		}


		dx = ddock_compute_dx(yc, rad, y+4);
		if (dx!=I_NAN)
			ddock_paint_hrow(bm, y+4, xc-dx+1, xc+dx+1, 0b1000000);

		board_buffer_draw(&act->bbuf[row]);
	}
}

int isqrt(int v)
{
	// horrible algorithm, but eh
	assert(v>=0);
	int root = 0;
	while (root*root < v)
	{
		root += 1;
	}
	return root-1;
}

int ddock_compute_dx(int yc, int r, int y)
{
	int dy = (y - yc);
	int dx2 = r*r - dy*dy;
	if (dx2 < 0)
		return I_NAN;
	return isqrt(dx2);
}

void ddock_paint_hrow(SSBitmap *bm, int y, int x0, int x1, SSBitmap mask)
{
#if 1 // fill
	int d0 = bound((x0+NUM_DIGITS*4/2)/4, 0, NUM_DIGITS-1);
	int d1 = bound((x1+NUM_DIGITS*4/2)/4, 0, NUM_DIGITS-1);
	int c;
	for (c=d0; c<=d1; c++)
	{
		bm[c] |= mask;
	}
#else	// outline
	int d0 = (x0+NUM_DIGITS*4/2)/4;
	if (d0>=0 && d0<NUM_DIGITS) { bm[d0] |= mask; }
	int d1 = (x1+NUM_DIGITS*4/2)/4;
	if (d1>=0 && d1<NUM_DIGITS) { bm[d1] |= mask; }
#endif
}

UIEventDisposition ddock_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	DDockAct *act = ((DDockHandler *) raw_handler)->act;

	switch (evt)
	{
		case '2':
			da_set_value(&act->yd, da_read(&act->yd)-1); break;
		case '8':
			da_set_value(&act->yd, da_read(&act->yd)+1); break;
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
		case uie_focus:
			act->focused = TRUE;
			da_set_velocity(&act->xd, 0);
			da_set_velocity(&act->yd, 0);
			da_set_velocity(&act->rd, 0);
			break;
		case uie_blur:
			act->focused = FALSE;
			break;
	}

	return uied_ignore;
}
