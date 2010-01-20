#include "disco.h"
#include "sound.h"

void disco_update(Disco *disco);
void disco_paint_once(Disco *disco);
UIEventDisposition disco_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void disco_init(Disco *disco, AudioClient *audioClient, ScreenBlanker *screenblanker)
{
	disco->func = (ActivationFunc) disco_update;
	disco->handler.func = (UIEventHandlerFunc) disco_event_handler;
	disco->handler.disco = disco;

	disco->audioClient = audioClient;
	disco->screenblanker = screenblanker;
	disco->focused = FALSE;

	schedule_us(1, (Activation*) disco);
}

void disco_update(Disco *disco)
{
	schedule_us(100000, (Activation*) disco);
	disco_paint_once(disco);
}

void disco_paint_once(Disco *disco)
{
	if (disco->focused)
	{
		screenblanker_setmode(disco->screenblanker, sb_disco);
		uint8_t disco_color = deadbeef_rand() % 6;
		//LOGF((logfp, "disco color %x\n", disco_color));
		screenblanker_setdisco(disco->screenblanker, disco_color);
	}
}

UIEventDisposition disco_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	Disco *disco = ((DiscoHandler *) raw_handler)->disco;

	UIEventDisposition result = uied_accepted;
	switch (evt)
	{
		case uie_focus:
			disco->focused = TRUE;
			break;
		case uie_escape:
			disco->focused = FALSE;
			screenblanker_setmode(disco->screenblanker, sb_inactive);
			result = uied_blur;
			break;
	}
	return result;
}
