#include "pong.h"
#include "rasters.h"

#define STUB(s)	{}

#define PS(v)	((v)<<PONG_SCALE2)
#define BALLDIA	3
#define PADDLEHEIGHT	8
#define SCORE_PAUSE_TIME (((Time)1)<<20)
#define BALLMINX	PS(2)
#define BALLMAXX	PS(29-BALLDIA)
#define BALLMINY	PS(0)
#define BALLMAXY	PS(24-BALLDIA)

void pong_serve(Pong *pong, uint8_t from_player);
void pong_update(Pong *pong);
void pong_paint_once(Pong *pong);
void pong_paint_paddle(Pong *pong, int x, int y);
void pong_score_one(Pong *pong, uint8_t player);
UIEventDisposition pong_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void pong_init(Pong *pong, uint8_t b0, FocusManager *focus)
{
	pong->func = (ActivationFunc) pong_update;
	int i;
	for (i=0; i<PONG_HEIGHT; i++)
	{
		board_buffer_init(&pong->bbuf[i]);
		board_buffer_push(&pong->bbuf[i], b0+i);
		pong->btable[i] = &pong->bbuf[i];
	}
	pong->handler.func = (UIEventHandlerFunc) pong_event_handler;
	pong->handler.pong = pong;

	pong->rrect.bbuf = pong->btable;
	pong->rrect.ylen = PONG_HEIGHT;
	pong->rrect.x = 0;
	pong->rrect.xlen = 8;

	focus_register(focus, (UIEventHandler*) &pong->handler, pong->rrect, "pong");

	pong_serve(pong, 0);

	pong->paddley[0] = 8;
	pong->paddley[1] = 10;

	pong->score[0] = 0;
	pong->score[1] = 0;
	pong->lastScore = clock_time_us();

	schedule_us(1, (Activation*) pong);
}

void pong_serve(Pong *pong, uint8_t from_player)
{
	pong->x = BALLMINX;
	pong->y = (deadbeef_rand() % (BALLMAXY - BALLMINY)) + BALLMINY;
	pong->dx = PS((deadbeef_rand() & 7)+7);
	pong->dy = PS((deadbeef_rand() & 7)+4);
	if (from_player)
	{
		pong->x = BALLMAXX;
		pong->dx = -pong->dx;
	}
}

uint8_t pong_is_paused(Pong *pong)
{
	Time dl = clock_time_us() - pong->lastScore;
	return (0<dl && dl<SCORE_PAUSE_TIME);
}

void pong_intersect_paddle(Pong *pong, uint8_t player, int by1)
{
	int by0 = pong->y;
	if (by0>by1)
	{
		int tmp = by1;
		by1 = by0;
		by0 = tmp;
	}
	int py0 = PS(pong->paddley[player]);
	int py1 = PS(pong->paddley[player]+PADDLEHEIGHT);
	if (by1<py0 || by0>py1)
	{
		STUB(sound_start(SOUND_PONG_SCORE));
		pong_score_one(pong, 1-player);
	}
	else
	{
		STUB(sound_start(SOUND_PONG_PADDLE_BOUNCE));
	}
}

void pong_advance_ball(Pong *pong)
{
	int newx = pong->x + (pong->dx >> PONG_FREQ2);
	int newy = pong->y + (pong->dy >> PONG_FREQ2);
	if (newx > BALLMAXX) {
		pong_intersect_paddle(pong, 1, newy);
		pong->dx = -pong->dx;
		newx = BALLMAXX-(newx-BALLMAXX);
	}
	if (newx < BALLMINX) {
		pong_intersect_paddle(pong, 0, newy);
		pong->dx = -pong->dx;
		newx = BALLMINX+(BALLMINX-newx);
	}
	if (newy> BALLMAXY) {
		pong->dy = -pong->dy;
		newy = BALLMAXY-(newy-BALLMAXY);
		STUB(sound_start(SOUND_PONG_WALL_BOUNCE));
	}
	if (newy< BALLMINY) {
		pong->dy = -pong->dy;
		newy = BALLMINY+(BALLMINY-newy);
		STUB(sound_start(SOUND_PONG_WALL_BOUNCE));
	}
	pong->x = newx;
	pong->y = newy;
}

void pong_score_one(Pong *pong, uint8_t player)
{
	pong->lastScore = clock_time_us();
	pong->score[player] += 1;
	pong_serve(pong, player);
	pong_paint_once(pong);
}

void pong_update(Pong *pong)
{
	schedule_us(1000000/PONG_FREQ, (Activation*) pong);
	if (!pong_is_paused(pong))
	{
		pong_advance_ball(pong);
	}
	pong_paint_once(pong);
}

void pong_paint_once(Pong *pong)
{
	raster_clear_buffers(&pong->rrect);

	if (pong_is_paused(pong))
	{
		ascii_to_bitmap_str(pong->bbuf[1].buffer+1, 6, "P0  P1");

		char scoretext[3];
		int_to_string2(scoretext, 2, 0, pong->score[0]);
		ascii_to_bitmap_str(pong->bbuf[2].buffer+1, 2, scoretext);

		int_to_string2(scoretext, 2, 0, pong->score[1]);
		ascii_to_bitmap_str(pong->bbuf[2].buffer+5, 2, scoretext);
	}
	else
	{
		int xo, yo;
		for (xo=0; xo<BALLDIA; xo++)
		{
			for (yo=0; yo<BALLDIA; yo++)
			{
				raster_paint_pixel(&pong->rrect, (pong->x >> PONG_SCALE2)+xo, (pong->y >> PONG_SCALE2)+yo);
			}
		}
	}

	pong_paint_paddle(pong, 0, pong->paddley[0]);
	pong_paint_paddle(pong, 28, pong->paddley[1]);

	raster_draw_buffers(&pong->rrect);
}

void pong_paint_paddle(Pong *pong, int x, int y)
{
	raster_paint_pixel(&pong->rrect, x+1, y);
	raster_paint_pixel(&pong->rrect, x+1, y+PADDLEHEIGHT);
	int yo;
	for (yo=1; yo<PADDLEHEIGHT; yo++)
	{
		raster_paint_pixel(&pong->rrect, x+0, y+yo);
		raster_paint_pixel(&pong->rrect, x+2, y+yo);
	}
}

UIEventDisposition pong_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	Pong *pong = ((PongHandler *) raw_handler)->pong;

	UIEventDisposition result = uied_accepted;
	switch (evt)
	{
		case '1':
			pong->paddley[0] = max(pong->paddley[0]-2, 0); break;
		case '4':
			pong->paddley[0] = min(pong->paddley[0]+2, 22-PADDLEHEIGHT); break;
		case '3':
			pong->paddley[1] = max(pong->paddley[1]-2, 0); break;
		case '6':
			pong->paddley[1] = min(pong->paddley[1]+2, 22-PADDLEHEIGHT); break;
		case uie_escape:
			result = uied_blur;
			break;
	}
	pong_paint_once(pong);
	return result;
}