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
int ddock_compute_dx(DDockAct *act, int y);
void ddock_paint_hrow(SSBitmap *bm, int y, int x0, int x1, SSBitmap mask);
int bound(int v, int l, int h);

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

	schedule(0, (Activation*) act);
}

void ddock_update(DDockAct *act)
{
	ddock_update_once(act);
	schedule(256, (Activation*) act);
}

#define I_NAN	 0x7fffffff

void ddock_update_once(DDockAct *act)
{
	if (!act->focused)
	{
		// standard animation
		act->r = bound(act->r + (deadbeef_rand() % 3) - 1, 7, 15);
		act->xc = bound(act->xc + (deadbeef_rand() % 3) - 1, -14, 14);
		act->yc = bound(act->yc + (deadbeef_rand() % 3) - 1, -15, 15);
	}

	int r;
	for (r=0; r<DOCK_HEIGHT; r++)
	{
		SSBitmap *bm = act->bbuf[r].buffer;
		int i;

		// draw axes
		SSBitmap bg = (r==(DOCK_HEIGHT/2)) ? 0b1000000 : 0;
		for (i=0; i<NUM_DIGITS; i++)	// zero buffer
		{
			bm[i] = bg;
		}
		bm[NUM_DIGITS/2] |= 0b0110000;

		int y = DOCK_HEIGHT*7/2 - (r*7);
		int dx;

		dx = ddock_compute_dx(act, y+0);
		if (dx!=I_NAN)
			ddock_paint_hrow(bm, y+0, act->xc-dx+1, act->xc+dx+1, 0b0001000);

		dx = ddock_compute_dx(act, y+1);
		if (dx!=I_NAN)
		{
			ddock_paint_hrow(bm, y+1, act->xc-dx+0, act->xc+dx+0, 0b0000100);
			ddock_paint_hrow(bm, y+1, act->xc-dx+2, act->xc+dx+2, 0b0010000);
		}
		dx = ddock_compute_dx(act, y+2);
		if (dx!=I_NAN)
			ddock_paint_hrow(bm, y+2, act->xc-dx+1, act->xc+dx+1, 0b0000001);

		dx = ddock_compute_dx(act, y+3);
		if (dx!=I_NAN)
		{
			ddock_paint_hrow(bm, y+3, act->xc-dx+0, act->xc+dx+0, 0b0000010);
			ddock_paint_hrow(bm, y+3, act->xc-dx+2, act->xc+dx+2, 0b0100000);
		}


		dx = ddock_compute_dx(act, y+4);
		if (dx!=I_NAN)
			ddock_paint_hrow(bm, y+4, act->xc-dx+1, act->xc+dx+1, 0b1000000);

		board_buffer_draw(&act->bbuf[r]);
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

int ddock_compute_dx(DDockAct *act, int y)
{
	int r = act->r;
	int dy = (y - act->yc);
	int dx2 = r*r - dy*dy;
	if (dx2 < 0)
		return I_NAN;
	return isqrt(dx2);
}

int bound(int v, int l, int h)
{
	if (v<l) { return l; }
	if (v>h) { return h; }
	return v;
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
			act->yc -= 1; break;
		case '8':
			act->yc += 1; break;
		case '4':
			act->xc -= 1; break;
		case '6':
			act->xc += 1; break;
		case 'p':
		case 'a':
			act->r += 1; break;
		case 's':
		case 'b':
			act->r -= 1; break;
		case uie_focus:
			act->focused = TRUE;
			break;
		case uie_blur:
			act->focused = FALSE;
			break;
	}

	return uied_ignore;
}
