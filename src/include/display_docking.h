#ifndef display_docking_h
#define display_docking_h

#include "clock.h"
#include "board_buffer.h"
#include "focus.h"
#include "drift_anim.h"
#include "thruster_protocol.h"
#include "screen4.h"
#include "audio_server.h"

#define DOCK_HEIGHT 4
#define MAX_Y (DOCK_HEIGHT*6)
#define MAX_X (NUM_DIGITS*4)
#define CTR_X (MAX_X/2)
#define CTR_Y (MAX_Y/2)

struct s_ddockact;

typedef struct {
	UIEventHandlerFunc func;
	struct s_ddockact *act;
} DDockHandler;

typedef struct {
	ThrusterUpdateFunc func;
	struct s_ddockact *act;
} DockThrusterUpdate;

typedef struct s_ddockact {
	ActivationFunc func;
	Screen4 s4;
	DDockHandler handler;
	DockThrusterUpdate thrusterUpdate;
	uint8_t focused;
	DriftAnim xd, yd, rd;
	uint32_t last_impulse_time;
	ThrusterPayload thrusterPayload;
	BoardBuffer auxboards[2];
	uint8_t auxboard_base;
	r_bool docking_complete;
	AudioClient *audioClient;
} DDockAct;

void ddock_init(DDockAct *act, uint8_t b0, uint8_t auxboard_base, FocusManager *focus, AudioClient *audioClient);
void ddock_reset(DDockAct *dd);

#endif // display_docking_h
