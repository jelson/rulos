#ifndef _disco_h
#define _disco_h

#include "rocket.h"
#include "audio_server.h"
#include "screenblanker.h"

typedef struct s_disco_handler {
	UIEventHandlerFunc func;
	struct s_disco *disco;
} DiscoHandler;

typedef struct s_disco {
	ActivationFunc func;
	ScreenBlanker *screenblanker;
	DiscoHandler handler;
	AudioClient *audioClient;
	r_bool focused;
} Disco;

void disco_init(Disco *disco, AudioClient *audioClient, ScreenBlanker *screenblanker);

#endif // _disco_h
