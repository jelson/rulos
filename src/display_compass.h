#ifndef display_compass_h
#define display_compass_h

#include "clock.h"
#include "board_buffer.h"
#include "focus.h"
#include "drift_anim.h"

struct s_dcompassact;

typedef struct {
	UIEventHandlerFunc func;
	struct s_dcompassact *act;
} DCompassHandler;

typedef struct s_dcompassact {
	ActivationFunc func;
	BoardBuffer bbuf;
	BoardBuffer *btable;	// needed for RectRegion
	DCompassHandler handler;
	uint8_t focused;
	DriftAnim drift;
	uint32_t last_impulse_time;
} DCompassAct;

void dcompass_init(DCompassAct *act, uint8_t board, FocusManager *focus);

#endif // display_compass_h
