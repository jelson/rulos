#include "screenblanker.h"

UIEventDisposition screenblanker_handler(ScreenBlanker *screenblanker, UIEvent evt);

void init_screenblanker(ScreenBlanker *screenblanker, BoardConfiguration bc, HPAM *hpam, IdleAct *idle)
{
	assert(bc==bc_rocket0 || bc==bc_rocket1);
	screenblanker->func = (UIEventHandlerFunc) screenblanker_handler;
	screenblanker->num_buffers = bc==bc_rocket0 ? 8 : 4;

	int i;
	for (i=0; i<screenblanker->num_buffers; i++)
	{
		board_buffer_init(&screenblanker->buffer[i]);
		int j;
		for (j=0; j<NUM_DIGITS; j++)
		{
			screenblanker->buffer[i].buffer[j] = SSB_DECIMAL;
		}
		if (hpam!=NULL && i==hpam->bbuf.board_index)
		{
			board_buffer_set_alpha(&screenblanker->buffer[i], ~hpam->bbuf.alpha);
		}
	}

	idle_add_handler(idle, (UIEventHandler *) screenblanker);
}

UIEventDisposition screenblanker_handler(ScreenBlanker *screenblanker, UIEvent evt)
{
	int i;
	if (evt == evt_idle_nowidle)
	{
		for (i=0; i<screenblanker->num_buffers; i++)
		{
			board_buffer_push(&screenblanker->buffer[i], i);
		}
	}
	else if (evt == evt_idle_nowactive)
	{
		for (i=0; i<screenblanker->num_buffers; i++)
		{
			board_buffer_pop(&screenblanker->buffer[i]);
		}
	}
	return uied_accepted;
}
